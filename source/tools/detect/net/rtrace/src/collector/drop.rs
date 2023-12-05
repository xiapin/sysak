mod bpf {
    include!(concat!(env!("OUT_DIR"), "/drop.skel.rs"));
    include!(concat!(env!("OUT_DIR"), "/drop.rs"));
}
use crate::common::ksyms::get_symbol_with_offset;
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
use std::sync::atomic::AtomicBool;
use std::sync::atomic::Ordering;

static DISABLE_TP_KFEE_SKB: AtomicBool = AtomicBool::new(false);

#[derive(Serialize, Deserialize, Debug)]
pub struct Drop {
    pub src: SocketAddr,
    pub dst: SocketAddr,
    pub sym: String,
    pub protocol: Protocol,
}

impl fmt::Display for Drop {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "{} {} -> {} {}",
            self.protocol, self.src, self.dst, self.sym
        )
    }
}

pub struct DropCollector<'a> {
    skel: bpf::DropSkel<'a>,
}

impl<'a> DropCollector<'a> {
    /// attach ping sender eBPF program
    pub fn new(
        verbose: bool,
        protocol: Protocol,
        saddr: u32,
        daddr: u32,
        sport: u16,
        dport: u16,
    ) -> Self {
        let mut builder = bpf::DropSkelBuilder::default();
        builder.obj_builder.debug(verbose);
        let mut opts = builder.obj_builder.opts(std::ptr::null());
        opts.btf_custom_path = btf_path_ptr();
        let mut open_skel = builder.open_opts(opts).unwrap();

        if !has_tp_kfree_skb() {
            open_skel
                .progs_mut()
                .tp_kfree_skb()
                .set_autoload(false)
                .unwrap();
        }

        let mut skel = open_skel
            .load()
            .expect("failed to load pingtrace sender program");

        // set filter map
        let filter = bpf::drop_filter {
            protocol: protocol as u16,
            saddr,
            daddr,
            sport,
            dport,
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
        DropCollector { skel }
    }

    pub fn poll(&mut self, mut tx: Sender<Event>) {
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
    let (head, body, _tail) = unsafe { data_vec.align_to::<bpf::drop_event>() };
    debug_assert!(head.is_empty(), "Data was not aligned");
    let event = body[0];

    let drop = Drop {
        src: SocketAddr::new(
            IpAddr::V4(Ipv4Addr::from(u32::from_be(event.saddr))),
            event.sport,
        ),
        dst: SocketAddr::new(
            IpAddr::V4(Ipv4Addr::from(u32::from_be(event.daddr))),
            event.dport,
        ),
        protocol: Protocol::try_from(event.proto as i32).unwrap(),
        sym: get_symbol_with_offset(&event.location),
    };
    tx.send(Event::Drop(drop)).expect("failed to send events");
}

pub fn disable_tp_kfree_skb() {
    DISABLE_TP_KFEE_SKB.store(true, Ordering::SeqCst);
}

pub fn has_tp_kfree_skb() -> bool {
    !DISABLE_TP_KFEE_SKB.load(Ordering::SeqCst)
}
