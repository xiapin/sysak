use crate::latency::skel::*;
use anyhow::{bail, Result};
use crate::latency::pidevent::PidEventType;
use crate::latency::sockevent::SockEventType;
use crate::latency::event::EventTypeTs;
use structopt::StructOpt;
use crate::common::*;
use crate::utils::LogDistribution;

#[derive(Debug, StructOpt)]
pub struct LatencyCommand {
    #[structopt(long, help = "Custom btf path")]
    btf: Option<String>,
    #[structopt(long, help = "Pid of tracing process")]
    pid: Option<u32>,
    #[structopt(long, help = "Local network address of tracing skb")]
    src: Option<String>,
    #[structopt(long, help = "Remote network address of tracing skb")]
    dst: Option<String>,
    #[structopt(long, help = "tracing skb with high latency in netstack")]
    netstack: bool,
    #[structopt(long, help = "tracing packet latency from kernel to application")]
    kernelapp: bool,

    #[structopt(long, default_value = "200", help = "latency threshold in ms")]
    threshold: u64,

    #[structopt(long, default_value = "3", help = "Period of display in seconds.")]
    period: u64,

    #[structopt(
        long,
        default_value = "10240",
        help = "Maximum number of tracing process"
    )]
    pidnum: u32,
    #[structopt(long, default_value = "10240", help = "Maximum number of tracing sock")]
    socknum: u32,
}

fn get_enabled_points(opts: &LatencyCommand) -> Vec<&str> {
    let mut enabled = vec![];
    if opts.netstack {
        enabled.push("pfifo_fast_enqueue");
        enabled.push("pfifo_fast_dequeue");
    }

    if opts.kernelapp {
        enabled.push("tcp_rcv_established");
        enabled.push("sock_def_readable");
        enabled.push("tp__sched_switch");
        enabled.push("tp__tcp_rcv_space_adjust");
        enabled.push("kprobe_tcp_v4_destroy_sock");
    }
    enabled
}

pub fn build_latency(opts: &LatencyCommand, debug: bool, btf: &Option<String>) -> Result<()> {
    // 1. get open skel
    let mut skel = Skel::default();
    let mut openskel = skel.open(debug, btf)?;

    // 2. set map size before load
    openskel
        .maps_mut()
        .sockmap()
        .set_max_entries(opts.socknum)?;
    openskel
        .maps_mut()
        .socktime_array()
        .set_max_entries(opts.socknum)?;
    openskel.maps_mut().pidmap().set_max_entries(opts.pidnum)?;
    openskel
        .maps_mut()
        .pidtime_array()
        .set_max_entries(opts.pidnum)?;
    // 3. load
    let mut enabled = get_enabled_points(opts);
    skel.load_enabled(openskel, enabled)?;
    // 4. set filter map
    let mut filter = Filter::new();
    filter.set_ap(&opts.src, &opts.dst)?;
    if let Some(pid) = opts.pid {
        filter.set_pid(pid);
    }
    filter.set_threshold(opts.threshold * 1000_000);
    skel.filter_map_update(0, unsafe {
        std::mem::transmute::<commonbinding::filter, filter>(filter.filter())
    })?;
    // 5. attach
    skel.attach()?;

    loop {
        if opts.netstack {
            // std::thread::sleep(std::time::Duration::from_secs(opts.period));
            // let ohist = latency.get_loghist()?;
            // if let Some(hist) = ohist {
            //     let logdis = hist.to_logdistribution();
            //     println!("{}", logdis);
            // }
        }

        if opts.kernelapp {
            if let Some(data) = skel.poll(std::time::Duration::from_millis(100))? {
                let event = Event::new(data);

                match event.event_type() {
                    EventType::LatencyEvent => {
                        let queue_ts = event.queue_ts();
                        let rcv_ts = event.rcv_ts();
                        let latency_ms = (rcv_ts - queue_ts) / 1_000_000;
                        let pidtime_array_idx = event.pidtime_array_idx();
                        let socktime_array_idx = event.socktime_array_idx();

                        let mut eventts_vec = Vec::new();

                        if let Some(x) = skel.pidtime_array_lookup(pidtime_array_idx)? {
                            for i in 0..PID_EVENTS_MAX {
                                let pidevent_ty = PidEventType::from(i);
                                for ts in tss_in_range(
                                    &x.pidevents[i as usize] as *const seconds4_ring
                                        as *const commonbinding::seconds4_ring,
                                    queue_ts,
                                    rcv_ts,
                                ) {
                                    eventts_vec.push(EventTypeTs::PidEvent(pidevent_ty, ts));
                                }
                            }
                        }

                        if let Some(x) = skel.socktime_array_lookup(socktime_array_idx)? {
                            for i in 0..SOCK_EVENTS_MAX {
                                let sockevent_ty = SockEventType::from(i);

                                for ts in tss_in_range(
                                    &x.sockevents[i as usize] as *const seconds4_ring
                                        as *const commonbinding::seconds4_ring,
                                    queue_ts,
                                    rcv_ts,
                                ) {
                                    eventts_vec.push(EventTypeTs::SockEvent(sockevent_ty, ts));
                                }
                            }
                        }

                        eventts_vec.push(EventTypeTs::KernelRcv(queue_ts));
                        eventts_vec.push(EventTypeTs::AppRcv(rcv_ts));
                        eventts_vec.sort_by(|a, b| a.ts().cmp(&b.ts()));

                        // pid/comm src -> dst ms
                        println!(
                            "{:>10}/{:<16} {:>25} -> {:<25} latency {}ms",
                            event.pid(),
                            event.comm(),
                            event.local(),
                            event.remote(),
                            latency_ms
                        );
                        print!("{}", eventts_vec[0]);
                        for i in 1..eventts_vec.len() {
                            print!(
                                "->{}-> {}",
                                (eventts_vec[i].ts() - eventts_vec[i - 1].ts()) / 1_000_000,
                                eventts_vec[i]
                            );
                        }
                        println!("");
                    }
                    _ => {
                        bail!("Unrecognized event type")
                    }
                }
            }
        }
    }
    Ok(())
}

pub struct Loghist {
    lh: loghist,
}

impl Loghist {
    pub fn zero() -> Loghist {
        Loghist {
            lh: unsafe { std::mem::MaybeUninit::zeroed().assume_init() },
        }
    }

    pub fn new(data: Vec<u8>) -> Loghist {
        let mut lh = unsafe { std::mem::MaybeUninit::zeroed().assume_init() };
        let ptr = &data[0] as *const u8 as *const loghist;

        unsafe {
            std::ptr::copy_nonoverlapping(ptr, &mut lh, 1);
        }

        Loghist { lh }
    }

    pub fn to_logdistribution(&self) -> LogDistribution {
        let mut logdis = LogDistribution::default();

        let mut idx = 0;
        for i in self.lh.hist {
            logdis.dis[idx] = i as usize;
            idx += 1;
        }

        logdis
    }

    pub fn vec(&self) -> Vec<u8> {
        unsafe {
            std::slice::from_raw_parts(
                &self.lh as *const loghist as *const u8,
                std::mem::size_of::<loghist>(),
            )
            .to_vec()
        }
    }
}
