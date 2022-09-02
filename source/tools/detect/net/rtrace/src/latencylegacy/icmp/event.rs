use crate::latencylegacy::bindings::*;
use std::fmt;

#[derive(Clone, Copy)]
pub enum IcmpEventType {
    Unkonw,
    PingSnd,
    PingNetDevQueue,
    PingNetDevXmit,
    PingDevRcv,
    PingNetifRcv,
    PingIcmpRcv,
    PingRcv,
    PingKfreeSkb,
}

impl From<u32> for IcmpEventType {
    fn from(value: u32) -> Self {
        match value {
            PING_SND => IcmpEventType::PingSnd,
            PING_NET_DEV_QUEUE => IcmpEventType::PingNetDevQueue,
            PING_NET_DEV_XMIT => IcmpEventType::PingNetDevXmit,
            PING_DEV_RCV => IcmpEventType::PingDevRcv,
            PING_NETIF_RCV => IcmpEventType::PingNetifRcv,
            PING_ICMP_RCV => IcmpEventType::PingIcmpRcv,
            PING_RCV => IcmpEventType::PingRcv,
            PING_KFREE_SKB => IcmpEventType::PingKfreeSkb,
            _ => IcmpEventType::Unkonw,
        }
    }
}

impl fmt::Display for IcmpEventType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let str;
        match &self {
            IcmpEventType::PingSnd => str = "ping_send",
            IcmpEventType::PingNetDevQueue => str = "dev_queue",
            IcmpEventType::PingNetDevXmit => str = "dev_xmit",
            IcmpEventType::PingDevRcv => str = "dev_rcv",
            IcmpEventType::PingNetifRcv => str = "netif_rcv",
            IcmpEventType::PingIcmpRcv => str = "icmp_rcv",
            IcmpEventType::PingRcv => str = "ping_rcv",
            IcmpEventType::PingKfreeSkb => str = "kfree",
            _ => str = "None",
        }
        write!(f, "{}", str)
    }
}

pub struct IcmpEvent {
    data: (usize, Vec<u8>),
    ptr: *const icmp_event,
}

impl IcmpEvent {
    pub fn new(data: (usize, Vec<u8>)) -> IcmpEvent {
        let mut e = IcmpEvent {
            ptr: &data.1[0] as *const u8 as *const icmp_event,
            data,
        };
        e
    }

    pub fn type_(&self) -> IcmpEventType {
        IcmpEventType::from(unsafe { (*self.ptr).type_ as u32 })
    }

    pub fn is_echo(&self) -> bool {
        unsafe { (*self.ptr).icmp_type == 8 }
    }

    pub fn is_echo_reply(&self) -> bool {
        unsafe { (*self.ptr).icmp_type == 0 }
    }

    pub fn seq(&self) -> u16 {
        unsafe { (*self.ptr).seq }
    }

    pub fn id(&self) -> u16 {
        unsafe { (*self.ptr).id }
    }

    pub fn ts(&self) -> u64 {
        unsafe { (*self.ptr).ts }
    }

    pub fn skb_ts(&self) -> u64 {
        unsafe { (*self.ptr).skb_ts }
    }
}

impl fmt::Display for IcmpEvent {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "id: {}, seq: {}, type: {}, is_echo: {}, ts: {}, skbts: {}",
            self.id(),
            self.seq(),
            self.type_(),
            self.is_echo(),
            self.ts(),
            self.skb_ts()
        )
    }
}
