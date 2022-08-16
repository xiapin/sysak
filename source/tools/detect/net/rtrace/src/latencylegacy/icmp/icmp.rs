use crate::bindings::*;
use crate::latencylegacy::icmp::{IcmpEvent, IcmpEventType};
use crate::icmpskel::*;
use crate::perf::PerfBufferBuilder;
use anyhow::{bail, Result};
use crossbeam_channel;
use crossbeam_channel::Receiver;
use eutils_rs::KernelVersion;
use libbpf_rs::MapFlags;
use once_cell::sync::Lazy;
use std::sync::Mutex;
use std::time::Duration;

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

pub struct Icmp<'a> {
    pub skel: IcmpSkel<'a>,
    rx: Option<Receiver<(usize, Vec<u8>)>>,
    filter: filter,
    delta: u64,
}

impl<'a> Icmp<'a> {
    pub fn new(debug: bool) -> Result<Icmp<'a>> {
        let skel = open_load_skel(debug)?;
        Ok(Icmp {
            skel,
            rx: None,
            delta: eutils_rs::timestamp::delta_of_mono_real_time(),
            filter: unsafe { std::mem::MaybeUninit::zeroed().assume_init() },
        })
    }

    pub fn attach(&mut self) -> Result<()> {
        self.skel.attach()?;
        Ok(())
    }

    pub fn poll(&mut self, timeout: Duration) -> Result<Option<IcmpEvent>> {
        if let Some(rx) = &self.rx {
            let data_warp = rx.recv_timeout(timeout);
            match data_warp {
                Ok(data) => {
                    let event = IcmpEvent::new(data);
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

fn open_load_skel<'a>(debug: bool) -> Result<IcmpSkel<'a>> {
    bump_memlock_rlimit()?;
    let mut skel_builder = IcmpSkelBuilder::default();
    skel_builder.obj_builder.debug(debug);
    let mut open_skel = skel_builder.open()?;
    
    // if KernelVersion::current()? < KernelVersion::try_from("3.11.0")? {
    //     open_skel
    //         .progs_mut()
    //         .kprobe__ping_v4_sendmsg()
    //         .set_autoload(false)?;
    // } else {
    //     open_skel
    //         .progs_mut()
    //         .kprobe__ping_sendmsg()
    //         .set_autoload(false)?;
    // }

    Ok(open_skel.load()?)
}

