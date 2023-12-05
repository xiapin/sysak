mod bpf {
    include!(concat!(env!("OUT_DIR"), "/virtio.skel.rs"));
    include!(concat!(env!("OUT_DIR"), "/virtio.rs"));
}
use crate::common::utils::any_as_u8_slice;
use crate::common::utils::btf_path_ptr;
use crate::common::utils::get_queue_count;
use crate::common::utils::get_send_receive_queue;
use libbpf_rs::skel::*;
use libbpf_rs::MapFlags;
use std::fs;

use serde::Deserialize;
use serde::Serialize;
use std::fmt;

#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct Queue {
    pub avail: u16,
    pub used: u16,
    pub last_used: u16,
    pub len: u16,
}

impl fmt::Display for Queue {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "avail-{}, used-{}, last_used-{}, len-{}",
            self.avail, self.used, self.last_used, self.len
        )
    }
}

#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct Virtio {
    pub rx: Vec<Queue>,
    pub tx: Vec<Queue>,
}

impl fmt::Display for Virtio {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let mut rxs = "RecvQueue ".to_owned();
        let mut txs = "SendQueue ".to_owned();
        for (_, rx) in self.rx.iter().enumerate() {
            rxs.push_str(&format!(
                "{:>03.2}%/{:<5} ",
                ((rx.used - rx.last_used) as f32 / rx.len as f32) * 100.0,
                rx.last_used
            ));
        }

        for (_, tx) in self.tx.iter().enumerate() {
            txs.push_str(&format!(
                "{:>03.2}%/{:<5} ",
                ((tx.avail - tx.used) as f32 / tx.len as f32) * 100.0,
                tx.last_used
            ));
        }

        write!(f, "{}\n{}", txs, rxs)
    }
}

pub struct VirtioCollector<'a> {
    skel: bpf::VirtioSkel<'a>,
    interface: String,
    tx_path: String,
    rx_path: String,
    rq_size: i32,
    sq_size: i32,
}

impl<'a> VirtioCollector<'a> {
    pub fn new(verbose: bool, interface: String) -> Self {
        let mut builder = bpf::VirtioSkelBuilder::default();
        builder.obj_builder.debug(verbose);
        let mut opts = builder.obj_builder.opts(std::ptr::null());
        opts.btf_custom_path = btf_path_ptr();
        let open_skel = builder.open_opts(opts).unwrap();

        let mut skel = open_skel.load().expect("failed to load virtio program");

        let mut mapval = unsafe { std::mem::zeroed::<bpf::virito_queue>() };
        mapval.pid = unsafe { libc::getpid() };
        let (sq, rq) = get_send_receive_queue().unwrap();
        mapval.sq_size = sq as i32;
        mapval.rq_size = rq as i32;
        skel.maps_mut()
            .imap()
            .update(
                &0_i32.to_ne_bytes(),
                unsafe { any_as_u8_slice(&mapval) },
                MapFlags::ANY,
            )
            .expect("failed to update userslow filter map");
        skel.attach().expect("failed to attach virtio program");
        VirtioCollector {
            skel,
            tx_path: format!("/sys/class/net/{}/dev_id", interface),
            rx_path: format!("/sys/class/net/{}/dev_port", interface),
            interface,
            sq_size: sq as i32,
            rq_size: rq as i32,
        }
    }

    fn trigger(&self) {
        let qc = get_queue_count(&self.interface).unwrap() / 2;
        for _ in 0..qc {
            fs::read_to_string(&self.tx_path).unwrap();
            fs::read_to_string(&self.rx_path).unwrap();
        }
    }

    fn reset_map(&mut self) {
        let mut mapval = unsafe { std::mem::zeroed::<bpf::virito_queue>() };
        mapval.pid = unsafe { libc::getpid() };
        mapval.sq_size = self.sq_size;
        mapval.rq_size = self.rq_size;
        self.skel
            .maps_mut()
            .imap()
            .update(
                &0_i32.to_ne_bytes(),
                unsafe { any_as_u8_slice(&mapval) },
                MapFlags::ANY,
            )
            .expect("failed to update userslow filter map");
    }

    fn read_map(&mut self) -> bpf::virito_queue {
        let ret = self
            .skel
            .maps_mut()
            .imap()
            .lookup(&0_i32.to_ne_bytes(), MapFlags::ANY)
            .expect("failed to read virtio map")
            .unwrap();
        let (head, body, _tail) = unsafe { ret.align_to::<bpf::virito_queue>() };
        debug_assert!(head.is_empty(), "Data was not aligned");
        let vq = body[0];
        vq
    }

    pub fn refresh(&mut self) -> Virtio {
        self.reset_map();
        self.trigger();
        let vq = self.read_map();

        let mut tx_queues = vec![];
        let mut rx_queues = vec![];
        for tx in 0..(vq.tx_idx as usize) {
            tx_queues.push(Queue {
                avail: vq.txs[tx].avail_idx,
                used: vq.txs[tx].used_idx,
                last_used: vq.txs[tx].last_used_idx,
                len: vq.txs[tx].len,
            });
        }

        for rx in 0..(vq.rx_idx as usize) {
            rx_queues.push(Queue {
                avail: vq.rxs[rx].avail_idx,
                used: vq.rxs[rx].used_idx,
                last_used: vq.rxs[rx].last_used_idx,
                len: vq.rxs[rx].len,
            });
        }

        let v = Virtio {
            tx: tx_queues,
            rx: rx_queues,
        };
        v
    }
}
