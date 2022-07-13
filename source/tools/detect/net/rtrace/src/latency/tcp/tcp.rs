use crate::bindings::*;
use crate::perf::PerfBufferBuilder;
use crate::latency::tcp::{AddressInfo, TcpEvent};
use crate::tcpskel::*;
use anyhow::{bail, Result};
use crossbeam_channel;
use crossbeam_channel::Receiver;
use libbpf_rs::MapFlags;
use once_cell::sync::Lazy;
use std::collections::HashMap;
use std::sync::Mutex;
use std::time::Duration;
use std::collections::BTreeMap;
use eutils_rs::KernelVersion;

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

fn open_load_skel<'a>(debug: bool) -> Result<TcpSkel<'a>> {
    bump_memlock_rlimit()?;
    let mut skel_builder = TcpSkelBuilder::default();
    skel_builder.obj_builder.debug(debug);
    let mut open_skel = skel_builder.open()?;
    if KernelVersion::current()? < KernelVersion::try_from("3.11.0")? {
        open_skel
            .progs_mut()
            .kprobe______ip_queue_xmit()
            .set_autoload(false)?;
    } else {
        open_skel
            .progs_mut()
            .kprobe__ip_queue_xmit()
            .set_autoload(false)?;
    }
    Ok(open_skel.load()?)
}

pub struct Tcp<'a> {
    skel: TcpSkel<'a>,
    rx: Option<Receiver<(usize, Vec<u8>)>>,
    filter: filter,
}

impl<'a> Tcp<'a> {
    pub fn new(debug: bool) -> Result<Tcp<'a>> {
        let skel = open_load_skel(debug)?;
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

    pub fn listend_socks_lookup(&mut self, addr: u64) -> Result<Option<AddressInfo>> {
        let res = self.skel.maps_mut().listend_socks().lookup(
            unsafe {
                std::slice::from_raw_parts(
                    &addr as *const u64 as *const u8,
                    std::mem::size_of::<u64>(),
                )
            },
            MapFlags::ANY,
        )?;
        if let Some(r) = res {
            return Ok(Some(AddressInfo::new(r)));
        }
        Ok(None)
    }

    pub fn poll(&mut self, timeout: Duration) -> Result<Option<TcpEvent>> {
        if let Some(rx) = &self.rx {
            let data_warp = rx.recv_timeout(timeout);
            match data_warp {
                Ok(data) => {
                    let event = TcpEvent::new(data);
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

