mod bpf {
    include!(concat!(env!("OUT_DIR"), "/retran.skel.rs"));
    include!(concat!(env!("OUT_DIR"), "/retran.rs"));
}
use crate::common::utils::btf_path_ptr;
use crate::common::utils::handle_lost_events;
use crate::event::Event;
use crossbeam_channel::Sender;
use libbpf_rs::skel::*;
use libbpf_rs::PerfBufferBuilder;
use serde::Deserialize;
use serde::Serialize;
use std::fmt;
use std::net::IpAddr;
use std::net::Ipv4Addr;
use std::net::SocketAddr;

#[derive(Serialize, Deserialize, Debug)]
pub struct Retran {
    pub tcp_state: String,
    pub ca_state: String,
    pub retran_type: String,
    pub src: SocketAddr,
    pub dst: SocketAddr,
}

impl fmt::Display for Retran {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "{} -> {} retran_type:{}",
            self.src, self.dst, self.retran_type
        )
    }
}

pub struct RetranCollector<'a> {
    skel: bpf::RetranSkel<'a>,
}

impl<'a> RetranCollector<'a> {
    /// attach ping sender eBPF program
    pub fn new(verbose: bool) -> Self {
        let mut builder = bpf::RetranSkelBuilder::default();
        builder.obj_builder.debug(verbose);
        let mut opts = builder.obj_builder.opts(std::ptr::null());
        opts.btf_custom_path = btf_path_ptr();
        let open_skel = builder.open_opts(opts).unwrap();
        let mut skel = open_skel.load().expect("failed to load retran program");
        skel.attach().expect("failed to attach retran program");
        RetranCollector { skel }
    }

    pub fn poll(&mut self, mut tx: Sender<Event>) {
        log::debug!("start retran polling thread");
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
    let (head, body, _tail) = unsafe { data_vec.align_to::<bpf::retran_event>() };
    debug_assert!(head.is_empty(), "Data was not aligned");
    let event = body[0];

    let ca_state = match event.ca_state {
        0 => "open",
        1 => "disorder",
        2 => "cwr",
        3 => "recovery",
        4 => "loss",
        _ => "none",
    };

    let retran_type = match event.retran_type {
        0 => "SynRetran",
        1 => "SlowStartRetran",
        2 => "RtoRetran",
        3 => "FastRetran",
        4 => "TLP",
        _ => "Other",
    };

    let retran = Retran {
        tcp_state: "".to_owned(),
        ca_state: ca_state.to_string(),
        retran_type: retran_type.to_owned(),
        src: SocketAddr::new(
            IpAddr::V4(Ipv4Addr::from(u32::from_be(event.saddr))),
            event.sport,
        ),
        dst: SocketAddr::new(
            IpAddr::V4(Ipv4Addr::from(u32::from_be(event.daddr))),
            event.dport,
        ),
    };
    tx.send(Event::Retran(retran))
        .expect("failed to send events");
}
