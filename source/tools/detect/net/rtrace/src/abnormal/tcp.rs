use crate::abnormal::event::Event;
use crate::abnormal::bindings::*;
use crate::abnormal::skel::*;
use crate::perf::PerfBufferBuilder;
use anyhow::{bail, Result};
use crossbeam_channel;
use crossbeam_channel::Receiver;
use once_cell::sync::Lazy;
use std::sync::Mutex;
use std::time::Duration;
use libbpf_rs::MapFlags;
use std::ffi::CString;

static GLOBAL_TX: Lazy<Mutex<Option<crossbeam_channel::Sender<(usize, Vec<u8>)>>>> =
    Lazy::new(|| Mutex::new(None));

pub fn handle_event(_cpu: i32, data: &[u8]) {
    let event = Vec::from(data);
    GLOBAL_TX
        .lock()
        .unwrap()
        .as_ref()
        .unwrap()
        .send((_cpu as usize, event))
        .unwrap();
}

pub fn handle_lost_events(cpu: i32, count: u64) {
    eprintln!("Lost {} events on CPU {}", count, cpu);
}

fn bump_memlock_rlimit() -> Result<()> {
    let rlimit = libc::rlimit {
        rlim_cur: 128 << 20,
        rlim_max: 128 << 20,
    };

    if unsafe { libc::setrlimit(libc::RLIMIT_MEMLOCK, &rlimit) } != 0 {
        bail!("Failed to increase rlimit");
    }

    Ok(())
}

fn open_load_skel<'a>(debug: bool, btf: &Option<String>) -> Result<AbnormalSkel<'a>> {
    let btf_cstring;
    let mut btf_cstring_ptr = std::ptr::null();
    if let Some(btf) = btf {
        btf_cstring = CString::new(btf.clone())?;
        btf_cstring_ptr = btf_cstring.as_ptr();
    }

    bump_memlock_rlimit()?;
    let mut skel_builder = AbnormalSkelBuilder::default();
    skel_builder.obj_builder.debug(debug);
    let mut open_opts = skel_builder.obj_builder.opts(std::ptr::null());
    open_opts.btf_custom_path = btf_cstring_ptr;
    let mut open_skel = skel_builder.open_opts(open_opts)?;
    Ok(open_skel.load()?)
}

pub struct Tcp<'a> {
    skel: AbnormalSkel<'a>,
    rx: Option<Receiver<(usize, Vec<u8>)>>,
    filter: filter,
}

impl<'a> Tcp<'a> {
    pub fn new(debug: bool, btf: &Option<String>) -> Result<Tcp<'a>> {
        let skel = open_load_skel(debug, btf)?;
        Ok(Tcp {
            skel,
            rx: None,
            filter: unsafe { std::mem::MaybeUninit::zeroed().assume_init() },
        })
    }

    pub fn set_filter_pid(&mut self, pid: u32) {
        self.filter.pid = pid;
    }

    pub fn set_filter_protocol(&mut self, protocol: u16) {
        self.filter.protocol = protocol;
    }

    pub fn set_filter_ap(&mut self, ap: addr_pair) {
        self.filter.ap = ap;
    }

    pub fn set_filter_ts(&mut self, ts: u64) {
        self.filter.ts = ts;
    }

    pub fn update_filter(&mut self) -> Result<()> {
        let mut key = vec![0; self.skel.maps().filter_map().key_size() as usize];
        let key_ptr = &mut key[0] as *mut u8 as *mut i32;
        unsafe {
            *key_ptr = 0;
        }
        self.skel.maps_mut().filter_map().update(
            &key,
            unsafe {
                std::slice::from_raw_parts(
                    &self.filter as *const filter as *const u8,
                    std::mem::size_of::<filter>(),
                )
            },
            MapFlags::ANY,
        )?;
        Ok(())
    }

    pub fn poll(&mut self, timeout: Duration) -> Result<Option<Event>> {
        if let Some(rx) = &self.rx {
            let data_warp = rx.recv_timeout(timeout);
            match data_warp {
                Ok(data) => {
                    let event = Event::new(data.1);
                    return Ok(Some(event));
                }
                _ => {
                    return Ok(None);
                }
            }
        }
        let (tx, rx) = crossbeam_channel::unbounded();
        self.rx = Some(rx);
        *GLOBAL_TX.lock().unwrap() = Some(tx);
        let perf = PerfBufferBuilder::new(self.skel.maps_mut().events())
            .sample_cb(handle_event)
            .lost_cb(handle_lost_events)
            .build()?;
        std::thread::spawn(move || loop {
            perf.poll(timeout).unwrap();
        });
        log::debug!("start successfully perf thread to receive event");
        Ok(None)
    }

    pub fn attach(&mut self) -> Result<()> {
        self.skel.attach()?;
        Ok(())
    }
}
