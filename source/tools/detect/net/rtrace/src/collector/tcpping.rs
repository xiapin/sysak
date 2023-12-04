use crate::common::ksyms::has_kernel_symbol;
use crate::common::sched::Process;
use crate::common::sched::Sched;
use crate::common::utils::allocate_port;
use crate::common::utils::any_as_u8_slice;
use crate::common::utils::current_monotime;
use crate::event::Event;
use crossbeam_channel::Sender;
use libbpf_rs::skel::*;
use pnet::packet::ip::IpNextHeaderProtocols;
use pnet::packet::tcp::MutableTcpPacket;
use pnet::packet::tcp::TcpFlags;
use pnet::transport::tcp_packet_iter;
use pnet::transport::transport_channel;
use pnet::transport::TransportChannelType::Layer4;
use pnet::transport::TransportProtocol::Ipv4;
use pnet::transport::TransportReceiver;
use pnet::transport::TransportSender;
use serde::Deserialize;
use serde::Serialize;
use std::collections::BTreeMap;
use std::fmt;
use std::net::Ipv4Addr;
use std::os::unix::io::AsFd;
use std::os::unix::io::AsRawFd;
use std::time::Duration;

include!(concat!(env!("OUT_DIR"), "/tcpping.skel.rs"));
include!(concat!(env!("OUT_DIR"), "/tcpping.rs"));

/// a life of tcpping packet
#[derive(Deserialize, Serialize, Debug, Clone)]
pub enum TcppingStage {
    TxUser,
    TxKernelIn,
    TxKernelOut,
    RxKernelIn,
    RxKernelOut,
    RxUser,
    Irq,
    SoftIrq,
    Sched(Sched),
}

impl TcppingStage {
    pub fn is_stage(&self, stage: &TcppingStage) -> bool {
        match (self, stage) {
            (TcppingStage::TxUser, TcppingStage::TxUser) => true,
            (TcppingStage::TxKernelIn, TcppingStage::TxKernelIn) => true,
            (TcppingStage::TxKernelOut, TcppingStage::TxKernelOut) => true,
            (TcppingStage::RxKernelIn, TcppingStage::RxKernelIn) => true,
            (TcppingStage::RxKernelOut, TcppingStage::RxKernelOut) => true,
            (TcppingStage::RxUser, TcppingStage::RxUser) => true,
            (TcppingStage::Irq, TcppingStage::Irq) => true,
            (TcppingStage::SoftIrq, TcppingStage::SoftIrq) => true,
            (TcppingStage::Sched(_), TcppingStage::Sched(_)) => true,
            _ => false,
        }
    }
}

impl fmt::Display for TcppingStage {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            TcppingStage::TxUser => write!(f, "TxUser"),
            TcppingStage::TxKernelIn => write!(f, "TxKernelIn"),
            TcppingStage::TxKernelOut => write!(f, "TxKernelOut"),
            TcppingStage::RxKernelIn => write!(f, "RxKernelIn"),
            TcppingStage::RxKernelOut => write!(f, "RxKernelOut"),
            TcppingStage::RxUser => write!(f, "RxUser"),
            TcppingStage::Irq => write!(f, "Irq"),
            TcppingStage::SoftIrq => write!(f, "SoftIrq"),
            TcppingStage::Sched(sched) => write!(f, "Sched({})", sched),
        }
    }
}

#[derive(Deserialize, Serialize, Debug, Default)]
pub struct Tcpping {
    pub seq: u32,
    pub stages: BTreeMap<u64, TcppingStage>,
}

impl Tcpping {
    pub fn stage_ts(&self, s: TcppingStage) -> u64 {
        for (ts, stage) in &self.stages {
            if stage.is_stage(&s) {
                return *ts;
            }
        }
        panic!("internal error");
    }

    pub fn scheds(&self) -> impl Iterator<Item = (&u64, &Sched)> {
        self.stages.iter().filter_map(|(ts, stage)| {
            if let TcppingStage::Sched(ref sched) = stage {
                Some((ts, sched))
            } else {
                None
            }
        })
    }

    pub fn irqs(&self) -> impl Iterator<Item = &u64> {
        self.stages.iter().filter_map(|(ts, stage)| {
            if let TcppingStage::Irq = stage {
                Some(ts)
            } else {
                None
            }
        })
    }

    pub fn delta(&self, left: TcppingStage, right: TcppingStage) -> u64 {
        self.stage_ts(left) - self.stage_ts(right)
    }

    pub fn time(&self) -> u64 {
        self.delta(TcppingStage::RxUser, TcppingStage::TxUser)
    }

    pub fn is_timeout(&self) -> bool {
        self.stages.len() == 0
    }

    pub(crate) fn add_stage(&mut self, ts: u64, stage: TcppingStage) {
        assert_ne!(ts, 0, "error:{}", stage);
        self.stages.insert(ts, stage);
    }

    pub(crate) fn add_irq(&mut self, ts: u64) {
        self.add_stage(ts, TcppingStage::Irq)
    }

    pub(crate) fn add_softirq(&mut self, ts: u64) {
        self.add_stage(ts, TcppingStage::SoftIrq)
    }

    pub(crate) fn add_sched(&mut self, ts: u64, sd: Sched) {
        self.add_stage(ts, TcppingStage::Sched(sd))
    }
}

impl fmt::Display for Tcpping {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mut string_vec = vec![];
        let mut prev_ts = 0;
        for (ts, name) in &self.stages {
            if prev_ts == 0 {
                string_vec.push(name.to_string());
                prev_ts = *ts;
                continue;
            }

            string_vec.push(format!("{}us", (*ts - prev_ts) / 1000));
            string_vec.push(name.to_string());
            prev_ts = *ts;
        }

        write!(f, "seq:{} {}", self.seq, string_vec.join(" -> "))
    }
}

pub struct TcppingCollector<'a> {
    skel: TcppingSkel<'a>,
    tx: TransportSender,
    rx: TransportReceiver,
}

impl<'a> TcppingCollector<'a> {
    pub fn new(verbose: bool) -> Self {
        // let interfaces = datalink::interfaces();
        let protocol = Layer4(Ipv4(IpNextHeaderProtocols::Tcp));
        let (tx, rx) = match transport_channel(4096, protocol) {
            Ok((tx, rx)) => (tx, rx),
            Err(e) => panic!(
                "An error occurred when creating the transport channel: {}",
                e
            ),
        };

        let mut skel_builder = TcppingSkelBuilder::default();
        skel_builder.obj_builder.debug(verbose);
        let mut open_skel = skel_builder.open().unwrap();

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

        let mut skel = open_skel.load().unwrap();

        skel.attach().unwrap();
        let prog = skel.progs().socket_tcp().as_fd().as_raw_fd();
        set_bpf_filter(rx.socket.fd, prog);

        TcppingCollector { skel, tx, rx }
    }

    fn set_filter(&mut self, sport: u16, dport: u16) {
        let mut filter = unsafe { std::mem::zeroed::<filter>() };
        filter.pid = unsafe { libc::getpid() };
        filter.be_lport = sport.to_be();
        filter.be_rport = dport.to_be();
        filter.lport = sport;
        filter.rport = dport;
        self.skel
            .maps_mut()
            .filters()
            .update(
                &0_u32.to_ne_bytes(),
                unsafe { any_as_u8_slice(&filter) },
                libbpf_rs::MapFlags::ANY,
            )
            .unwrap();
    }

    pub fn ping(
        &mut self,
        tx: Sender<Event>,
        interval: Duration,
        count: u32,
        sport: u16,
        dport: u16,
        src: Ipv4Addr,
        dst: Ipv4Addr,
    ) {
        for seq in 0..count {
            let new_sport = allocate_port(sport);
            assert_ne!(new_sport, 0, "failed to allocate local port");
            self.set_filter(new_sport, dport);

            let seq = seq + 1;
            let send_ts = current_monotime();
            match self.ping_once(seq, new_sport, dport, src, dst) {
                Some(_) => {
                    let mut tp = self.do_collect(seq);
                    tp.add_stage(send_ts, TcppingStage::TxUser);
                    tx.send(Event::Tcpping(tp)).unwrap();
                    std::thread::sleep(interval);
                }
                None => {
                    let mut tp = Tcpping::default();
                    tp.seq = seq;
                    tx.send(Event::Tcpping(tp)).unwrap();
                }
            }
        }
    }

    fn do_collect(&mut self, seq: u32) -> Tcpping {
        let ts = current_monotime();
        match self
            .skel
            .maps_mut()
            .latency()
            .lookup(&0_u32.to_ne_bytes(), libbpf_rs::MapFlags::ANY)
        {
            Ok(Some(data)) => {
                let (head, body, _tail) = unsafe { data.align_to::<tcpping>() };
                debug_assert!(head.is_empty(), "Data was not aligned");
                let tp = body[0];
                let mut new_tp = Tcpping::default();

                let mut cnt = tp.irq.cnt as usize;
                for i in 0..(IRQ_RING_SIZE as usize) {
                    if cnt <= i {
                        break;
                    }
                    let idx = (cnt - i - 1) & ((IRQ_RING_SIZE as usize) - 1);
                    new_tp.add_irq(tp.irq.tss[idx]);
                }

                cnt = tp.sirq.cnt as usize;
                for i in 0..(SOFTIRQ_RING_SIZE as usize) {
                    if cnt <= i {
                        break;
                    }
                    let idx = (cnt - i - 1) & ((SOFTIRQ_RING_SIZE as usize) - 1);
                    new_tp.add_softirq(tp.sirq.tss[idx]);
                }

                cnt = tp.sched.cnt as usize;
                for i in 0..(SCHEDSWITCH_RING_SIZE as usize) {
                    if cnt <= i {
                        break;
                    }
                    let idx = (cnt - i - 1) & ((SCHEDSWITCH_RING_SIZE as usize) - 1);
                    let ss = &tp.sched.ss[idx];
                    let prev = Process::new(ss.prev_pid as u32, ss.prev_comm.to_vec());
                    let next = Process::new(ss.next_pid as u32, ss.next_comm.to_vec());
                    new_tp.add_sched(ss.ts, Sched::new(prev, next));
                }

                new_tp.add_stage(
                    tp.stages[TCPPING_STAGE_TCPPING_TX_ENTRY as usize].ts,
                    TcppingStage::TxKernelIn,
                );
                new_tp.add_stage(
                    tp.stages[TCPPING_STAGE_TCPPING_TX_EXIT as usize].ts,
                    TcppingStage::TxKernelOut,
                );
                new_tp.add_stage(
                    tp.stages[TCPPING_STAGE_TCPPING_RX_ENTRY as usize].ts,
                    TcppingStage::RxKernelIn,
                );
                new_tp.add_stage(
                    tp.stages[TCPPING_STAGE_TCPPING_RX_EXIT as usize].ts,
                    TcppingStage::RxKernelOut,
                );

                new_tp.add_stage(ts, TcppingStage::RxUser);
                new_tp.seq = seq;

                let zero_tp = unsafe { std::mem::zeroed::<tcpping>() };
                self.skel
                    .maps_mut()
                    .latency()
                    .update(
                        &0_u32.to_ne_bytes(),
                        unsafe { any_as_u8_slice(&zero_tp) },
                        libbpf_rs::MapFlags::ANY,
                    )
                    .unwrap();

                return new_tp;
            }
            Ok(None) => {
                panic!("unexpected");
            }
            Err(e) => {
                panic!("error: {}", e);
            }
        }
    }

    fn ping_once(
        &mut self,
        seq: u32,
        sport: u16,
        dport: u16,
        src: Ipv4Addr,
        dst: Ipv4Addr,
    ) -> Option<()> {
        let tcp = build_tcphdr(seq, sport, dport, src, dst);
        self.tx.send_to(tcp, std::net::IpAddr::V4(dst)).unwrap();
        let mut iter = tcp_packet_iter(&mut self.rx);
        loop {
            match iter.next_with_timeout(std::time::Duration::from_secs(1)) {
                Ok(None) => {
                    log::debug!("timeout: failed to receive syn+ack packet");
                    return None;
                }
                Ok(Some((p, _))) => {
                    if p.get_destination() == sport && p.get_acknowledgement() == seq + 1 {
                        log::debug!("receive response, seq: {}", seq);
                        return Some(());
                    } else {
                        log::debug!("unexpected packet: {:?}", p);
                    }
                }
                Err(e) => {
                    panic!("error: {}", e);
                }
            }
        }
    }
}

fn build_tcphdr(
    seq: u32,
    sport: u16,
    dport: u16,
    src: Ipv4Addr,
    dst: Ipv4Addr,
) -> MutableTcpPacket<'static> {
    let mut tcp_header = MutableTcpPacket::owned(vec![0u8; 20]).unwrap();
    tcp_header.set_source(sport);
    tcp_header.set_destination(dport);

    tcp_header.set_flags(TcpFlags::SYN);
    tcp_header.set_window(64240);
    tcp_header.set_data_offset(5);
    tcp_header.set_urgent_ptr(0);
    tcp_header.set_sequence(seq);
    let checksum = pnet::packet::tcp::ipv4_checksum(&tcp_header.to_immutable(), &src, &dst);
    tcp_header.set_checksum(checksum);
    tcp_header
}

fn set_bpf_filter(sock: i32, prog: i32) {
    unsafe {
        assert_eq!(
            libc::setsockopt(
                sock,
                libc::SOL_SOCKET,
                libc::SO_ATTACH_BPF,
                &prog as *const i32 as *const libc::c_void,
                4,
            ),
            0,
            "failed to bind eBPF program into socket"
        );
    };
}
