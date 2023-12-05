mod bpf {
    include!(concat!(env!("OUT_DIR"), "/ping_sender.skel.rs"));
    include!(concat!(env!("OUT_DIR"), "/pingtrace.rs"));
}

use crate::common::btree::CpuBTreeMap;
use crate::common::btree::CpuBTreeSet;
use crate::common::ksyms::has_kernel_symbol;
use crate::common::raw_event::RawEvent;
use crate::common::sched::Process;
use crate::common::sched::Sched;
use crate::common::utils::btf_path_ptr;
use crate::common::utils::handle_lost_events;
use crate::event::Event;
use bpf::PING_SENDER_STAGE_PING_DEV_QUEUE;
use bpf::PING_SENDER_STAGE_PING_DEV_XMIT;
use bpf::PING_SENDER_STAGE_PING_ICMP_RCV;
use bpf::PING_SENDER_STAGE_PING_NETIF_RCV;
use bpf::PING_SENDER_STAGE_PING_RCV;
use bpf::PING_SENDER_STAGE_PING_SND;
use byteorder::ByteOrder;
use crossbeam_channel::Receiver;
use crossbeam_channel::Sender;
use libbpf_rs::skel::*;
use libbpf_rs::PerfBufferBuilder;
use serde::Deserialize;
use serde::Serialize;
use std::collections::BTreeMap;
use std::fmt;

/// a life of ping packet
#[derive(Deserialize, Serialize, Debug)]
pub enum PingStage {
    PingSnd,
    PingDevQueue,
    PingDevXmit,
    PingNetifRcv,
    PingIcmpRcv,
    PingRcv,
    Irq,
    SoftIrq,
    Wakeup,
    Sched(Sched),
}

impl fmt::Display for PingStage {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            PingStage::PingSnd => write!(f, "Send"),
            PingStage::PingDevQueue => write!(f, "DevQueue"),
            PingStage::PingDevXmit => write!(f, "DevXmit"),
            PingStage::PingNetifRcv => write!(f, "NetifRcv"),
            PingStage::PingIcmpRcv => write!(f, "IcmpRcv"),
            PingStage::PingRcv => write!(f, "Recv"),
            PingStage::Irq => write!(f, "Irq"),
            PingStage::SoftIrq => write!(f, "SoftIrq"),
            PingStage::Wakeup => write!(f, "WakeupKosfitrqd"),
            PingStage::Sched(s) => write!(f, "SchedSwitch({})", s),
        }
    }
}

/// The actual path information of ping packets
#[derive(Deserialize, Serialize, Debug)]
pub struct Ping {
    id: u16,
    seq: u16,
    stages: BTreeMap<u64, PingStage>,
}

impl Ping {
    pub(crate) fn new(id: u16, seq: u16) -> Self {
        Ping {
            id,
            seq,
            stages: Default::default(),
        }
    }

    pub(crate) fn add_stage(&mut self, ts: u64, stage: PingStage) {
        self.stages.insert(ts, stage);
    }

    pub(crate) fn add_irq(&mut self, ts: u64) {
        self.add_stage(ts, PingStage::Irq)
    }

    pub(crate) fn add_softirq(&mut self, ts: u64) {
        self.add_stage(ts, PingStage::SoftIrq)
    }
    pub(crate) fn add_wakeup(&mut self, ts: u64) {
        self.add_stage(ts, PingStage::Wakeup)
    }
    pub(crate) fn add_sched(&mut self, ts: u64, sd: Sched) {
        self.add_stage(ts, PingStage::Sched(sd))
    }
}

impl fmt::Display for Ping {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let mut string_vec = vec![];
        let mut prev_ts = 0;
        for (k, v) in self.stages.iter() {
            if prev_ts == 0 {
                string_vec.push(format!("{}", v));
                prev_ts = *k;
                continue;
            }

            string_vec.push(format!("{}us", (*k - prev_ts) / 1000));
            string_vec.push(format!("{}", v));
            prev_ts = *k;
        }

        write!(
            f,
            "Id:{} Seq:{} {}",
            self.id,
            self.seq,
            string_vec.join(" -> ")
        )
    }
}

fn __handle_event(tx: &mut Sender<RawEvent>, cpu: i32, data: &[u8]) {
    log::debug!("receive perf buffer event");
    let ty = byteorder::NativeEndian::read_u64(data) as u32;
    let data_vec = data.to_vec();

    let raw_event = RawEvent {
        cpu,
        ty,
        data: data_vec,
    };

    tx.send(raw_event).expect("failed to send events");
}

/// wrapper of ping sender skeleton
pub struct PingSender<'a> {
    skel: bpf::PingSenderSkel<'a>,
}

impl<'a> PingSender<'a> {
    /// attach ping sender eBPF program
    pub fn new(verbose: bool) -> Self {
        let mut builder = bpf::PingSenderSkelBuilder::default();
        builder.obj_builder.debug(verbose);
        let mut opts = builder.obj_builder.opts(std::ptr::null());
        opts.btf_custom_path = btf_path_ptr();
        let mut open_skel = builder.open_opts(opts).unwrap();

        // check network card type
        if has_kernel_symbol("skb_recv_done") {
            log::debug!("detect network card: virtio");
            open_skel
                .progs_mut()
                .kprobe__mlx5e_completion_event()
                .set_autoload(false)
                .unwrap();
        } else if has_kernel_symbol("mlx5e_completion_event") {
            log::debug!("detect network card: mlx5");
            open_skel
                .progs_mut()
                .kprobe__skb_recv_done()
                .set_autoload(false)
                .unwrap();
        } else {
            log::error!("detect network card: unknown, only support virtio and mlx5");
            panic!();
        }

        let mut skel = open_skel
            .load()
            .expect("failed to load pingtrace sender program");
        skel.attach()
            .expect("failed to attach pingtrace sender program");
        PingSender { skel }
    }

    // Poll the raw event of perf for ping sender
    fn inernal_poll_thread(&mut self, mut tx: Sender<RawEvent>) {
        let handle_event = move |cpu: i32, data: &[u8]| {
            __handle_event(&mut tx, cpu, data);
        };
        let perf = PerfBufferBuilder::new(&self.skel.maps_mut().perf_events())
            .sample_cb(handle_event)
            .lost_cb(handle_lost_events)
            .build()
            .unwrap();
        log::debug!("start pingtrace perf buffer polling thread");
        std::thread::spawn(move || loop {
            perf.poll(std::time::Duration::from_millis(200)).unwrap();
        });
    }

    pub fn poll(&mut self, tx: Sender<Event>) {
        log::debug!("start pingtrace polling thread");
        let (itx, irx) = crossbeam_channel::unbounded();
        self.inernal_poll_thread(itx);
        // do polling
        do_poll_thread(tx, irx);
    }
}

fn do_poll_thread(tx: Sender<Event>, irx: Receiver<RawEvent>) {
    let cpus = num_cpus::get();
    let mut irqs = CpuBTreeSet::<u64>::new(cpus);
    let mut softirqs = CpuBTreeSet::<u64>::new(cpus);
    let mut wakeups = CpuBTreeSet::<u64>::new(cpus);
    let mut scheds = CpuBTreeMap::<u64, Sched>::new(cpus);

    loop {
        match irx.recv_timeout(std::time::Duration::from_millis(200)) {
            Ok(raw_event) => {
                if let Some(e) = handle_raw_event(
                    raw_event,
                    &mut irqs,
                    &mut softirqs,
                    &mut wakeups,
                    &mut scheds,
                ) {
                    tx.send(Event::Ping(e)).expect("Faild to send event");
                }
            }
            Err(_) => {}
        }
    }
}

fn handle_raw_event(
    raw_event: RawEvent,
    irqs: &mut CpuBTreeSet<u64>,
    softirqs: &mut CpuBTreeSet<u64>,
    wakeups: &mut CpuBTreeSet<u64>,
    scheds: &mut CpuBTreeMap<u64, Sched>,
) -> Option<Ping> {
    macro_rules! record_raw_event {
        ($raw_event: ident, $ty: ident, $tree: ident) => {
            let cpu = $raw_event.cpu as usize;
            let (head, body, _tail) = unsafe { $raw_event.data.align_to::<bpf::$ty>() };
            debug_assert!(head.is_empty(), "Data was not aligned");
            let event_raw = body[0];

            for ts in event_raw.tss {
                $tree.insert(cpu, ts);
            }
        };
    }

    match raw_event.ty {
        bpf::EVENT_TYPE_PING => {
            let (head, body, _tail) = unsafe { raw_event.data.align_to::<bpf::ping_sender>() };
            debug_assert!(head.is_empty(), "Data was not aligned");
            let ping = body[0];
            let pt = handle_ping_event(ping, irqs, softirqs, wakeups, scheds);
            return Some(pt);
        }
        bpf::EVENT_TYPE_IRQ => {
            record_raw_event!(raw_event, irq, irqs);
        }
        bpf::EVENT_TYPE_SOFTIRQ => {
            record_raw_event!(raw_event, softirq, softirqs);
        }
        bpf::EVENT_TYPE_WAKEUP => {
            record_raw_event!(raw_event, wakeup, wakeups);
        }
        bpf::EVENT_TYPE_SCHED => handle_sched_event(scheds, &raw_event),
        _ => panic!(
            "Unknown event type: {}, data: {:?}",
            raw_event.ty, raw_event.data
        ),
    }

    None
}

fn handle_ping_event(
    ping: bpf::ping_sender,
    irqs: &mut CpuBTreeSet<u64>,
    softirqs: &mut CpuBTreeSet<u64>,
    wakeups: &mut CpuBTreeSet<u64>,
    scheds: &mut CpuBTreeMap<u64, Sched>,
) -> Ping {
    let mut pt = Ping::new(ping.key.id, ping.key.seq);

    let send_ts = ping.stages[PING_SENDER_STAGE_PING_SND as usize].ts;
    let dev_queue_ts = ping.stages[PING_SENDER_STAGE_PING_DEV_QUEUE as usize].ts;
    let dev_xmit_ts = ping.stages[PING_SENDER_STAGE_PING_DEV_XMIT as usize].ts;
    let netif_rcv_ts = ping.stages[PING_SENDER_STAGE_PING_NETIF_RCV as usize].ts;
    let netif_rcv_cpu = ping.stages[PING_SENDER_STAGE_PING_NETIF_RCV as usize].cpu as usize;
    let icmp_rcv_ts = ping.stages[PING_SENDER_STAGE_PING_ICMP_RCV as usize].ts;
    let recv_ts = ping.stages[PING_SENDER_STAGE_PING_RCV as usize].ts;

    assert_ne!(send_ts, 0);
    assert_ne!(dev_queue_ts, 0);
    assert_ne!(dev_xmit_ts, 0);
    assert_ne!(netif_rcv_ts, 0);
    assert_ne!(icmp_rcv_ts, 0);
    assert_ne!(recv_ts, 0);

    pt.add_stage(send_ts, PingStage::PingSnd);
    pt.add_stage(dev_queue_ts, PingStage::PingDevQueue);
    pt.add_stage(dev_xmit_ts, PingStage::PingDevXmit);
    pt.add_stage(netif_rcv_ts, PingStage::PingNetifRcv);
    pt.add_stage(icmp_rcv_ts, PingStage::PingIcmpRcv);
    pt.add_stage(recv_ts, PingStage::PingRcv);

    let mut irq_ts = 0;
    if let Some(irq) = irqs.lower_bound(netif_rcv_cpu, netif_rcv_ts) {
        pt.add_irq(*irq);
        irq_ts = *irq;
    } else {
        log::warn!("Hardware irq lost");
    }

    if irq_ts != 0 {
        for si in softirqs.in_range(netif_rcv_cpu, irq_ts, netif_rcv_ts) {
            pt.add_softirq(si);
        }
    }

    let mut sched_start_ts = 0;
    if irq_ts != 0 {
        if let Some(sched) = scheds.lower_bound(netif_rcv_cpu, irq_ts) {
            sched_start_ts = *sched.0;
            pt.add_stage(sched_start_ts, PingStage::Sched(sched.1.clone()));
        }
    }

    for (ts, sched) in scheds.range(netif_rcv_cpu, sched_start_ts, netif_rcv_ts) {
        pt.add_sched(*ts, sched.clone());
    }

    if let Some((ts, sched)) = scheds.lower_bound(netif_rcv_cpu, netif_rcv_ts) {
        if sched.next.comm.starts_with("ksoftirqd") {
            // find wakeup
            if let Some(wakeup_ts) = wakeups.lower_bound(netif_rcv_cpu, *ts) {
                pt.add_wakeup(*wakeup_ts);
            }
        }
    }

    let split_ts = netif_rcv_ts - 3_000_000_000;
    irqs.flush(netif_rcv_cpu, split_ts);
    softirqs.flush(netif_rcv_cpu, split_ts);
    wakeups.flush(netif_rcv_cpu, split_ts);
    scheds.flush(netif_rcv_cpu, split_ts);

    pt
}

fn handle_sched_event(scheds: &mut CpuBTreeMap<u64, Sched>, raw_event: &RawEvent) {
    let cpu = raw_event.cpu as usize;
    let (head, body, _tail) = unsafe { raw_event.data.align_to::<bpf::sched>() };
    debug_assert!(head.is_empty(), "Data was not aligned");
    let event_raw = body[0];

    let cnt = event_raw.cnt as usize;
    for i in 0..(bpf::SCHEDSWITCH_RING_SIZE as usize) {
        if cnt <= i {
            break;
        }

        let idx = (cnt - i - 1) & ((bpf::SCHEDSWITCH_RING_SIZE as usize) - 1);
        let ts = event_raw.ss[idx].ts;
        if scheds.contains_key(cpu, &ts) {
            continue;
        }

        let prev = Process::new(
            event_raw.ss[idx].prev_pid as u32,
            event_raw.ss[idx].prev_comm.to_vec(),
        );
        let next = Process::new(
            event_raw.ss[idx].next_pid as u32,
            event_raw.ss[idx].next_comm.to_vec(),
        );

        let sched = Sched::new(prev, next);

        scheds.insert(cpu, ts, sched);
    }
}
