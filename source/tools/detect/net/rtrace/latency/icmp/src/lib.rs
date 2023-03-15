mod bindings {
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}

mod skel {
    include!(concat!(env!("OUT_DIR"), "/icmp.skel.rs"));
}

mod icmp;

pub use {icmp::Icmp, skel::OpenIcmpSkel};

use bindings::*;
use cenum_rs::CEnum;
use std::fmt;
use serde::{Deserialize, Serialize};

#[derive(Clone, Copy, CEnum, Serialize, Deserialize)]
pub enum IcmpEventType {
    #[cenum(value = "PING_SND", display = "PingSend")]
    PingSnd,
    #[cenum(value = "PING_NET_DEV_QUEUE", display = "DevQueue")]
    PingNetDevQueue,
    #[cenum(value = "PING_NET_DEV_XMIT", display = "DevXmit")]
    PingNetDevXmit,
    #[cenum(value = "PING_DEV_RCV", display = "DevRcv")]
    PingDevRcv,
    #[cenum(value = "PING_NETIF_RCV", display = "NetifRcv")]
    PingNetifRcv,
    #[cenum(value = "PING_ICMP_RCV", display = "IcmpRcv")]
    PingIcmpRcv,
    #[cenum(value = "PING_RCV", display = "PingRcv")]
    PingRcv,
}


#[derive(Serialize, Deserialize)]
pub struct IcmpEvent {
    pub cpu: u16,
    pub ty: IcmpEventType,
    pub ts: u64,
    pub pid: u32,
    pub comm: String,
}

impl IcmpEvent {
    pub(crate) fn new(cpu: u16, ty: IcmpEventType, ts: u64) -> Self {
        IcmpEvent {
            cpu,
            ty,
            ts,
            pid: 0,
            comm: "0".to_owned(),
        }
    }

    pub fn set_pid(&mut self, pid: u32) {
        self.pid = pid;
    }

    pub fn set_comm(&mut self, comm: String) {
        self.comm = comm;
    }

    // pub fn set_comm(&mut self, comm: ) {

    // }

    pub fn ts(&self) -> u64 {
        self.ts
    }
}

#[derive(Serialize, Deserialize)]
pub struct IcmpEvents {
    pub sender: bool,
    pub id: u16,
    pub seq: u16,
    pub events: Vec<IcmpEvent>,
}

impl IcmpEvents {
    pub(crate) fn new(sender: bool, id: u16, seq: u16) -> Self {
        IcmpEvents {
            sender,
            id,
            seq,
            events: vec![],
        }
    }

    pub fn sender_ts(&self) -> u64 {
        todo!()
    }

    pub fn receiver_ts(&self) -> u64 {
        todo!()
    }

    pub fn start_ts(&self) -> u64 {
        self.events[0].ts
    }

    pub fn end_ts(&self) -> u64 {
        self.events.last().unwrap().ts
    }

    pub fn duration(&self) -> u64 {
        self.end_ts() - self.start_ts()
    }

    pub(crate) fn push(&mut self, event: IcmpEvent) {
        self.events.push(event);
    }

    pub(crate) fn sort(&mut self) {
        self.events.sort_by(|a, b| a.ts().cmp(&b.ts()));
    }

    pub fn valid(&self) -> bool {
        !self.events.is_empty()
    }
}

impl fmt::Display for IcmpEvents {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.events.len() == 0 {
            return write!(f, "");
        }

        if self.sender {
            write!(f, "sender: ")?;
        } else {
            write!(f, "receiver: ")?;
        }

        write!(f, "id: {} seq: {}, ", self.id, self.seq)?;
        let mut pre_ts = self.events[0].ts;
        write!(f, "{}({},{}/{})", self.events[0].ty, self.events[0].cpu, self.events[0].pid, self.events[0].comm)?;
        for i in 1..self.events.len() {
            let delta = (self.events[i].ts - pre_ts) / 1000;
            pre_ts = self.events[i].ts;
            write!(
                f,
                "-> {}us ->{}({},{}/{})",
                delta,
                self.events[i].ty,
                self.events[i].cpu,
                self.events[i].pid,
                self.events[i].comm
            )?;
        }
        write!(f, "")
    }
}
