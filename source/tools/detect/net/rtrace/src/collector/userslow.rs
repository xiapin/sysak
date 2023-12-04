mod bpf {
    include!(concat!(env!("OUT_DIR"), "/userslow.skel.rs"));
    include!(concat!(env!("OUT_DIR"), "/userslow.rs"));
}
use crate::common::sched::Process;
use crate::common::sched::Sched;
use crate::common::utils::btf_path_ptr;
use crate::common::utils::handle_lost_events;
use crate::event::Event;
use crossbeam_channel::Sender;
use libbpf_rs::skel::*;
use libbpf_rs::MapFlags;
use libbpf_rs::PerfBufferBuilder;
use serde::Deserialize;
use serde::Serialize;
use std::fmt;
use std::net::IpAddr;
use std::net::Ipv4Addr;
use std::net::SocketAddr;

#[derive(Deserialize, Serialize, Debug)]
pub struct Userslow {
    saddr: SocketAddr,
    daddr: SocketAddr,
    sched: Sched,

    kernel_recv_ts: u64,
    sched_ts: u64,
    user_recv_ts: u64,
}

impl fmt::Display for Userslow {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let mut string_vec = vec![];

        let delta_us;
        if self.sched_ts < self.kernel_recv_ts {
            string_vec.push(self.sched.to_string());
            string_vec.push(format!(
                "{}us",
                (self.kernel_recv_ts - self.sched_ts) / 1000
            ));
            string_vec.push("KernelRcv".to_owned());
            delta_us = (self.user_recv_ts - self.kernel_recv_ts) / 1000;
        } else {
            string_vec.push("KernelRcv".to_owned());
            string_vec.push(format!(
                "{}us",
                (self.sched_ts - self.kernel_recv_ts) / 1000
            ));
            string_vec.push(self.sched.to_string());
            delta_us = (self.user_recv_ts - self.sched_ts) / 1000;
        }

        string_vec.push(format!("{}us", delta_us));
        string_vec.push("UserRcv".to_owned());

        write!(
            f,
            "{} -> {}  {}",
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
        skel.maps_mut()
            .filter()
            .update(
                &0_i32.to_ne_bytes(),
                &threshold.to_ne_bytes(),
                MapFlags::ANY,
            )
            .expect("failed to update userslow filter map");
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

    let prev = Process::new(event.sched.prev_pid as u32, event.sched.prev_comm.to_vec());
    let next = Process::new(event.sched.next_pid as u32, event.sched.next_comm.to_vec());
    let us = Userslow {
        saddr: SocketAddr::new(
            IpAddr::V4(Ipv4Addr::from(u32::from_be(event.saddr))),
            event.sport,
        ),
        daddr: SocketAddr::new(
            IpAddr::V4(Ipv4Addr::from(u32::from_be(event.daddr))),
            event.dport,
        ),
        sched: Sched::new(prev, next),

        kernel_recv_ts: event.krcv_ts,
        sched_ts: event.sched.ts,
        user_recv_ts: event.urcv_ts,
    };
    tx.send(Event::UserSlow(us)).expect("failed to send events");
}
