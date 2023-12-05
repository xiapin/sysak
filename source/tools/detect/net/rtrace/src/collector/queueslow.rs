mod bpf {
    include!(concat!(env!("OUT_DIR"), "/queueslow.skel.rs"));
    include!(concat!(env!("OUT_DIR"), "/queueslow.rs"));
}
use crate::common::protocol::Protocol;
use crate::common::utils::any_as_u8_slice;
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
pub struct QueueSlow {
    saddr: SocketAddr,
    daddr: SocketAddr,
    protocol: Protocol,
    delay: u64,
}

impl fmt::Display for QueueSlow {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "{} {} -> {}  {}us",
            self.protocol,
            self.saddr,
            self.daddr,
            self.delay / 1000
        )
    }
}

pub struct QueueSlowCollector<'a> {
    skel: bpf::QueueslowSkel<'a>,
}

impl<'a> QueueSlowCollector<'a> {
    /// attach ping sender eBPF program
    pub fn new(verbose: bool, protocol: Protocol, threshold: u64) -> Self {
        let mut builder = bpf::QueueslowSkelBuilder::default();
        builder.obj_builder.debug(verbose);
        let mut opts = builder.obj_builder.opts(std::ptr::null());
        opts.btf_custom_path = btf_path_ptr();
        let mut open_skel = builder.open_opts(opts).unwrap();

        let mut skel = open_skel
            .load()
            .expect("failed to load pingtrace sender program");
        // set filter map
        let filter = bpf::filter {
            protocol: protocol as u32,
            threshold,
        };
        skel.maps_mut()
            .filters()
            .update(
                &0_i32.to_ne_bytes(),
                unsafe { any_as_u8_slice(&filter) },
                MapFlags::ANY,
            )
            .expect("failed to update userslow filter map");
        skel.attach()
            .expect("failed to attach pingtrace sender program");
        QueueSlowCollector { skel }
    }

    pub fn poll(&mut self, mut tx: Sender<Event>) {
        log::debug!("start queueslow polling thread");
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
    let (head, body, _tail) = unsafe { data_vec.align_to::<bpf::queue_slow>() };
    debug_assert!(head.is_empty(), "Data was not aligned");
    let event = body[0];

    let qs = QueueSlow {
        saddr: SocketAddr::new(
            IpAddr::V4(Ipv4Addr::from(u32::from_be(event.saddr))),
            event.sport,
        ),
        daddr: SocketAddr::new(
            IpAddr::V4(Ipv4Addr::from(u32::from_be(event.daddr))),
            event.dport,
        ),
        protocol: Protocol::try_from(event.protocol as i32).unwrap(),
        delay: event.latency,
    };
    tx.send(Event::QueueSlow(qs))
        .expect("failed to send events");
}
