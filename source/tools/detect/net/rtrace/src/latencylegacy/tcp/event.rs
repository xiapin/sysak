use crate::latencylegacy::bindings::*;
use anyhow::Result;
use std::fmt;
use std::net::{IpAddr, Ipv4Addr, SocketAddr, SocketAddrV4};

#[derive(Debug, Clone, Copy)]
pub enum TcpEventType {
    Unkonw,
    TcpSendMsg,
    TcpTransmitSkb,
    TcpIpQueueXmit,
    TcpNetDevXmit,
    TcpDevRcv,
    TcpQueueRcv,
    TcpCleanupRbuf,
    KfreeSkb,
    TcpAck,
}

impl From<u32> for TcpEventType {
    fn from(value: u32) -> Self {
        match value {
            TCP_SEND_MSG => TcpEventType::TcpSendMsg,
            TCP_TRANSMIT_SKB => TcpEventType::TcpTransmitSkb,
            TCP_IP_QUEUE_XMIT => TcpEventType::TcpIpQueueXmit,
            TCP_NET_DEV_XMIT => TcpEventType::TcpNetDevXmit,
            TCP_DEV_RCV => TcpEventType::TcpDevRcv,
            TCP_QUEUE_RCV => TcpEventType::TcpQueueRcv,
            TCP_CLEANUP_RBUF => TcpEventType::TcpCleanupRbuf,
            TCP_ACK => TcpEventType::TcpAck,
            _ => TcpEventType::Unkonw,
        }
    }
}

impl fmt::Display for TcpEventType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let str;
        match &self {
            TcpEventType::TcpSendMsg => str = "tcp_snd",
            TcpEventType::TcpTransmitSkb => str = "tcp_xmit",
            TcpEventType::TcpIpQueueXmit => str = "ip_queue",
            TcpEventType::TcpNetDevXmit => str = "dev_xmit",
            TcpEventType::TcpDevRcv => str = "dev_rcv",
            TcpEventType::TcpQueueRcv => str = "tcp_rcv",
            TcpEventType::TcpCleanupRbuf => str = "usr_rcv",
            TcpEventType::TcpAck => str = "ack",
            _ => str = "None",
        }
        write!(f, "{}", str)
    }
}

pub struct TcpEvent {
    data: (usize, Vec<u8>),
    ptr: *const tcp_event,
}

impl TcpEvent {
    pub fn new(data: (usize, Vec<u8>)) -> TcpEvent {
        let mut e = TcpEvent {
            ptr: &data.1[0] as *const u8 as *const tcp_event,
            data,
        };
        e
    }

    pub fn type_(&self) -> TcpEventType {
        TcpEventType::from(unsafe { (*self.ptr).type_ } as u32)
    }

    pub fn sockaddr(&self) -> u64 {
        unsafe { (*self.ptr).sockaddr }
    }
    pub fn skbaddr(&self) -> u64 {
        unsafe { (*self.ptr).skbaddr }
    }
    pub fn ts(&self) -> u64 {
        unsafe { (*self.ptr).ts }
    }

    pub fn skbts(&self) -> u64 {
        unsafe { (*self.ptr).skbts }
    }

    pub fn seq(&self) -> u32 {
        unsafe { (*self.ptr).seq }
    }

    pub fn end_seq(&self) -> u32 {
        unsafe { (*self.ptr).end_seq }
    }

    pub fn snd_una(&self) -> u32 {
        unsafe { (*self.ptr).snd_una }
    }

    pub fn ack_seq(&self) -> u32 {
        unsafe { (*self.ptr).ack_seq }
    }
}

impl fmt::Display for TcpEvent {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "TcpEvent type: {}, seq: {}, end_seq: {}, ack_seq: {}, sockaddr: {}, skbaddr: {}, ts:{}, skbts: {}", self.type_(), self.seq(), self.end_seq(), self.ack_seq(), self.sockaddr(), self.skbaddr(), self.ts(), self.skbts())
    }
}

pub struct TcpUsrEvent {
    ts: u64,
    seq: (usize, usize),
    type_: TcpEventType,
}

impl TcpUsrEvent {
    pub fn new(ts: u64, seq: (usize, usize), type_: TcpEventType) -> TcpUsrEvent {
        TcpUsrEvent { ts, seq, type_ }
    }

    pub fn ts(&self) -> u64 {
        self.ts
    }

    pub fn seq(&self) -> (usize, usize) {
        self.seq
    }

    pub fn type_(&self) -> TcpEventType {
        self.type_
    }
}

impl fmt::Display for TcpUsrEvent {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "TcpUsrEvent type: {}, seq: ({}, {}), ts: {}",
            self.type_(),
            self.seq().0,
            self.seq().1,
            self.ts()
        )
    }
}

pub struct AddressInfo {
    data: Vec<u8>,
    ptr: *const address_info,
}

impl AddressInfo {
    pub fn new(data: Vec<u8>) -> AddressInfo {
        AddressInfo {
            ptr: &data[0] as *const u8 as *const address_info,
            data,
        }
    }

    pub fn pid(&self) -> u32 {
        unsafe { (*self.ptr).pi.pid }
    }

    pub fn comm(&self) -> String {
        unsafe { String::from_utf8_unchecked((*self.ptr).pi.comm.to_vec()) }
    }

    pub fn addr_pair(&self) -> (SocketAddr, SocketAddr) {
        let daddr = unsafe { (*self.ptr).ap.daddr };
        let dport = unsafe { (*self.ptr).ap.dport };
        let saddr = unsafe { (*self.ptr).ap.saddr };
        let sport = unsafe { (*self.ptr).ap.sport };
        let src = SocketAddr::new(IpAddr::V4(Ipv4Addr::from(u32::from_be(saddr))), sport);
        let dst = SocketAddr::new(IpAddr::V4(Ipv4Addr::from(u32::from_be(daddr))), dport);
        (src, dst)
    }
}

impl fmt::Display for AddressInfo {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let ap = self.addr_pair();
        write!(f, "{}:{} {} -> {}", self.pid(), self.comm(), ap.0, ap.1,)
    }
}

pub fn string_to_addr_pair(src: &String, dst: &String) -> Result<addr_pair> {
    let s: SocketAddrV4 = src.parse()?;
    let d: SocketAddrV4 = dst.parse()?;
    Ok(addr_pair {
        saddr: u32::from_le_bytes(s.ip().octets()),
        daddr: u32::from_le_bytes(d.ip().octets()),
        sport: s.port(),
        dport: d.port(),
    })
}
