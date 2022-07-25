use crate::perf::PerfBufferBuilder;
use crate::sli::event::{Event, LatencyHist};
use crate::sli_bindings::*;
use crate::sliskel::*;
use anyhow::{bail, Result};
use crossbeam_channel;
use crossbeam_channel::Receiver;
use eutils_rs::proc::Kallsyms;
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
    pub fn new(debug: bool, threshold: u32) -> Result<Sli<'a>> {
        let skel = open_load_skel(debug)?;
        let mut zero_latency_hist: latency_hist =
            unsafe { std::mem::MaybeUninit::zeroed().assume_init() };
        zero_latency_hist.threshold = threshold;
        Ok(Sli {
            skel,
            rx: None,
            zero_latency_hist,
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

    /// update with zero map value.
    pub fn lookup_and_update_latency_map(&mut self, key: u32) -> Result<Option<LatencyHist>> {
        let map_key = unsafe {
            std::slice::from_raw_parts(&key as *const u32 as *const u8, std::mem::size_of::<u32>())
        };
        let map_value = unsafe {
            std::slice::from_raw_parts(
                &self.zero_latency_hist as *const latency_hist as *const u8,
                std::mem::size_of::<latency_hist>(),
            )
        };
        let res = self
            .skel
            .maps_mut()
            .latency_map()
            .lookup(map_key, MapFlags::ANY)?;
        if let Some(r) = res {
            self.skel
                .maps_mut()
                .latency_map()
                .update(map_key, map_value, MapFlags::ANY)?;
            return Ok(Some(LatencyHist::new(
                &r[0] as *const u8 as *const latency_hist,
            )));
        }
        Ok(None)
    }

    pub fn attach_latency(&mut self) -> Result<()> {
        let ksyms = Kallsyms::try_from("/proc/kallsyms")?;
        if ksyms.has_sym("tcp_rtt_estimator") {
            self.skel.links.kprobe__tcp_rtt_estimator =
                Some(self.skel.progs_mut().kprobe__tcp_rtt_estimator().attach()?);
        } else {
            self.skel.links.kprobe__tcp_ack =
                Some(self.skel.progs_mut().kprobe__tcp_ack().attach()?);
        }
        Ok(())
    }

    pub fn attach_applatency(&mut self) -> Result<()> {
        self.skel.links.tp__tcp_probe = Some(self.skel.progs_mut().tp__tcp_probe().attach()?);
        self.skel.links.tp__tcp_rcv_space_adjust =
            Some(self.skel.progs_mut().tp__tcp_rcv_space_adjust().attach()?);
        Ok(())
    }
}
