mod bpf {
    include!(concat!(env!("OUT_DIR"), "/userslow.skel.rs"));
    include!(concat!(env!("OUT_DIR"), "/userslow.rs"));
}
use crate::common::sched::Process;
use crate::common::sched::Sched;
use crate::common::utils::btf_path_ptr;
use crate::common::utils::handle_lost_events;
use crate::event::Event;
use crate::filter::Filter;
use crossbeam_channel::Sender;
use libbpf_rs::skel::*;
use libbpf_rs::MapFlags;
use libbpf_rs::PerfBufferBuilder;
use serde::Deserialize;
use serde::Serialize;
use std::collections::BTreeMap;
use std::fmt;
use std::net::IpAddr;
use std::net::Ipv4Addr;
use std::net::SocketAddr;

#[derive(Deserialize, Serialize, Debug)]
pub enum UserslowStage {
    KernelRcv,
    Wakeup(u16),
    SchedIn(u16),
    SchedOut(u16),
    UserRcv,
}

impl fmt::Display for UserslowStage {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            UserslowStage::KernelRcv => write!(f, "KernelRcv"),
            UserslowStage::Wakeup(ref value) => write!(f, "Wakeup({})", value),
            UserslowStage::SchedIn(ref value) => write!(f, "SchedIn({})", value),
            UserslowStage::SchedOut(ref value) => write!(f, "SchedOut({})", value),
            UserslowStage::UserRcv => write!(f, "UserRcv"),
        }
    }
}

#[derive(Deserialize, Serialize, Debug)]
pub struct Userslow {
    saddr: SocketAddr,
    daddr: SocketAddr,
    proc: Process,
    stages: BTreeMap<u64, UserslowStage>,
}

impl Userslow {
    pub fn new(event: &bpf::slow_event) -> Self {
        let mut stages = BTreeMap::<u64, UserslowStage>::default();
        stages.insert(event.krcv_ts, UserslowStage::KernelRcv);
        stages.insert(event.urcv_ts, UserslowStage::UserRcv);

        let saddr = SocketAddr::new(
            IpAddr::V4(Ipv4Addr::from(u32::from_be(event.saddr))),
            event.sport,
        );
        let daddr = SocketAddr::new(
            IpAddr::V4(Ipv4Addr::from(u32::from_be(event.daddr))),
            event.dport,
        );
        let proc = Process::new(event.sched.next_pid as u32, event.sched.next_comm.to_vec());

        let cnt = event.thread.cnt as usize;
        for i in 0..(bpf::MAX_ITEM_NUM as usize) {
            if cnt <= i {
                break;
            }
            let idx = (cnt - i - 1) & ((bpf::MAX_ITEM_NUM as usize) - 1);
            let item = &event.thread.items[idx];
            let stage = match item.ty as u32 {
                bpf::THREAD_ITEM_TYPE_SCHED_IN => UserslowStage::SchedIn(item.cpu),
                bpf::THREAD_ITEM_TYPE_SCHED_OUT => UserslowStage::SchedOut(item.cpu),
                bpf::THREAD_ITEM_TYPE_WAKE_UP => UserslowStage::Wakeup(item.cpu),
                _ => panic!("BUG: internal"),
            };
            stages.insert(item.ts, stage);
        }
        Userslow {
            saddr,
            daddr,
            proc,
            stages,
        }
    }
}

impl fmt::Display for Userslow {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
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

        write!(
            f,
            "process:{} {}->{} {}",
            self.proc,
            self.saddr,
            self.daddr,
            string_vec.join(" -> ")
        )
    }
}

pub struct UserSlowCollector<'a> {
    skel: bpf::UserslowSkel<'a>,
}

impl<'a> UserSlowCollector<'a> {
    /// attach ping sender eBPF program
    pub fn new(verbose: bool, threshold: u64) -> Self {
        let mut builder = bpf::UserslowSkelBuilder::default();
        builder.obj_builder.debug(verbose);
        let mut opts = builder.obj_builder.opts(std::ptr::null());
        opts.btf_custom_path = btf_path_ptr();
        let open_skel = builder.open_opts(opts).unwrap();
        let mut skel = open_skel
            .load()
            .expect("failed to load pingtrace sender program");

        let mut filter = Filter::new(skel.maps().filters());
        filter.set_threshold(threshold);
        filter.update();

        skel.attach()
            .expect("failed to attach pingtrace sender program");
        UserSlowCollector { skel }
    }

    pub fn poll(&mut self, mut tx: Sender<Event>) {
        log::debug!("start userslow polling thread");
        let handle_event = move |cpu: i32, data: &[u8]| {
            __handle_event(&mut tx, cpu, data);
        };

        let perf = PerfBufferBuilder::new(&self.skel.maps_mut().perf_events())
            .sample_cb(handle_event)
            .lost_cb(handle_lost_events)
            .build()
            .unwrap();

        loop {
            perf.poll(std::time::Duration::from_millis(200)).unwrap();
        }
    }
}

fn __handle_event(tx: &mut Sender<Event>, _cpu: i32, data: &[u8]) {
    let data_vec = data.to_vec();
    let (head, body, _tail) = unsafe { data_vec.align_to::<bpf::slow_event>() };
    debug_assert!(head.is_empty(), "Data was not aligned");
    let event = body[0];
    let us = Userslow::new(&event);
    tx.send(Event::UserSlow(us)).expect("failed to send events");
}
