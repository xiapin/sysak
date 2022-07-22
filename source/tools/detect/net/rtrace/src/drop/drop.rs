use crate::drop::event::Event;
use crate::drop_bindings::*;
use crate::dropskel::*;
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

fn open_load_skel<'a>(debug: bool, btf: &Option<String>) -> Result<DropSkel<'a>> {
    let btf_cstring;
    let mut btf_cstring_ptr = std::ptr::null();
    if let Some(btf) = btf {
        btf_cstring = CString::new(btf.clone())?;
        btf_cstring_ptr = btf_cstring.as_ptr();
    }

    bump_memlock_rlimit()?;
    let mut skel_builder = DropSkelBuilder::default();
    skel_builder.obj_builder.debug(debug);
    let mut open_opts = skel_builder.obj_builder.opts(std::ptr::null());
    open_opts.btf_custom_path = btf_cstring_ptr;
    let mut open_skel = skel_builder.open_opts(open_opts)?;
    Ok(open_skel.load()?)
}

pub struct Drop<'a> {
    skel: DropSkel<'a>,
    rx: Option<Receiver<(usize, Vec<u8>)>>,
}

impl<'a> Drop<'a> {
    pub fn new(debug: bool, btf: &Option<String>) -> Result<Drop<'a>> {
        let skel = open_load_skel(debug, btf)?;
        Ok(Drop { skel, rx: None })
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

    pub fn get_stack(&mut self, stackid: i32) -> Result<Vec<u8>> {
        let res = self.skel.maps_mut().stackmap().lookup(
            unsafe { std::slice::from_raw_parts(&stackid as *const i32 as *const u8, 4) },
            MapFlags::ANY,
        )?;

        match res {
            Some(x) => Ok(x),
            None => bail!("failed to get stack from stackid - {}", stackid),
        }
    }

    pub fn attach(&mut self) -> Result<()> {
        self.skel.attach()?;
        Ok(())
    }
}
