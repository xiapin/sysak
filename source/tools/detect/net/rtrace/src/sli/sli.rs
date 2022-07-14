use crate::perf::PerfBufferBuilder;
use crate::sli::event::{Event, LatencyHist};
use crate::sli_bindings::*;
use crate::sliskel::*;
use anyhow::{bail, Result};
use crossbeam_channel;
use crossbeam_channel::Receiver;
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

fn open_load_skel<'a>(debug: bool) -> Result<SliSkel<'a>> {
    bump_memlock_rlimit()?;
    let mut skel_builder = SliSkelBuilder::default();
    skel_builder.obj_builder.debug(debug);
    let mut open_skel = skel_builder.open()?;
    Ok(open_skel.load()?)
}

pub struct Sli<'a> {
    skel: SliSkel<'a>,
    rx: Option<Receiver<(usize, Vec<u8>)>>,
    zero_latency_hist: latency_hist,
}

impl<'a> Sli<'a> {
    pub fn new(debug: bool) -> Result<Sli<'a>> {
        let skel = open_load_skel(debug)?;
        Ok(Sli {
            skel,
            rx: None,
            zero_latency_hist: unsafe { std::mem::MaybeUninit::zeroed().assume_init() },
        })
    }

    pub fn poll(&mut self, timeout: Duration) -> Result<Option<Event>> {
        if let Some(rx) = &self.rx {
            let data_warp = rx.recv_timeout(timeout);
            match data_warp {
                Ok(data) => {
                    let event = Event::new(&data.1[0] as *const u8 as *const event)?;
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

    pub fn lookup_and_delte_latency_map(&mut self) -> Result<Option<LatencyHist>> {
        let key: u32 = 0;
        let res = self.skel.maps_mut().latency_map().lookup(
            unsafe {
                std::slice::from_raw_parts(
                    &key as *const u32 as *const u8,
                    std::mem::size_of::<u32>(),
                )
            },
            MapFlags::ANY,
        )?;
        if let Some(r) = res {
            return Ok(Some(LatencyHist::new(
                &r[0] as *const u8 as *const latency_hist,
            )));
        }
        Ok(None)
    }

    pub fn attach_latency(&mut self) -> Result<()> {
        self.skel.links.kprobe__tcp_ack = Some(self.skel.progs_mut().kprobe__tcp_ack().attach()?);
        Ok(())
    }
}
