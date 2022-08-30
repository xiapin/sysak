#[path = "bindings/commonbinding.rs"]
pub mod commonbinding;

use anyhow::{bail, Result};
use commonbinding::*;
use eutils_rs::net::ProtocolType;
use std::net::{IpAddr, Ipv4Addr, SocketAddr, SocketAddrV4};

pub enum EventType {
    DropKfreeSkb,
    DropTcpDrop,
    DropIptables,
    DropNfconntrackDrop,

    LatencyEvent,
    ConnectLatencyEvent,
    Unknown,
}

impl From<u32> for EventType {
    fn from(value: u32) -> Self {
        match value {
            DROP_KFREE_SKB => EventType::DropKfreeSkb,
            DROP_TCP_DROP => EventType::DropTcpDrop,
            DROP_IPTABLES_DROP => EventType::DropIptables,
            DROP_NFCONNTRACK_DROP => EventType::DropNfconntrackDrop,
            LATENCY_EVENT => EventType::LatencyEvent,
            CONNECT_LATENCY_EVENT => EventType::ConnectLatencyEvent,
            _ => EventType::Unknown,
        }
    }
}

/// Filter
pub struct Filter {
    pub filter: filter,
}

impl Filter {
    pub fn new() -> Filter {
        Filter {
            filter: unsafe { std::mem::MaybeUninit::zeroed().assume_init() },
        }
    }

    pub fn set_pid(&mut self, pid: u32) {
        self.filter.pid = pid;
    }

    pub fn set_ap(&mut self, src: &Option<String>, dst: &Option<String>) -> Result<()> {
        self.set_src(src)?;
        self.set_dst(dst)
    }

    pub fn set_src(&mut self, src: &Option<String>) -> Result<()> {
        if let Some(x) = src {
            let s: SocketAddrV4 = x.parse()?;
            self.filter.ap.saddr = u32::from_le_bytes(s.ip().octets());
            self.filter.ap.sport = s.port();
        }
        Ok(())
    }

    pub fn set_dst(&mut self, dst: &Option<String>) -> Result<()> {
        if let Some(x) = dst {
            let sock: SocketAddrV4 = x.parse()?;
            self.filter.ap.saddr = u32::from_le_bytes(sock.ip().octets());
            self.filter.ap.sport = sock.port();
        }
        Ok(())
    }

    pub fn set_protocol(&mut self, protocol: u16) {
        self.filter.protocol = protocol;
    }

    pub fn set_threshold(&mut self, threshold: u64) {
        self.filter.threshold = threshold;
    }

    pub fn to_vec(&self) -> Vec<u8> {
        unsafe {
            std::slice::from_raw_parts(
                &self.filter as *const filter as *const u8,
                std::mem::size_of::<filter>(),
            )
            .to_vec()
        }
    }

    pub fn filter(&self) -> filter {
        self.filter.clone()
    }
}

pub struct Event {
    // event: event,
    data: (usize, Vec<u8>),
    pub ptr: *const event,
    score: Option<u32>
}

impl Event {
    pub fn new(data: (usize, Vec<u8>)) -> Event {
        Event {
            ptr: &data.1[0] as *const u8 as *const event,
            data,
            score: None,
        }
    }

    pub fn event_type(&self) -> EventType {
        EventType::from(unsafe { (*self.ptr).type_ as u32 })
    }

    pub fn pid(&self) -> u32 {
        unsafe { (*self.ptr).pid }
    }

    pub fn comm(&self) -> String {
        unsafe { String::from_utf8_unchecked((*self.ptr).comm.to_vec()) }
    }

    pub fn local(&self) -> SocketAddr {
        let saddr = unsafe { (*self.ptr).ap.saddr };
        let sport = unsafe { (*self.ptr).ap.sport };
        SocketAddr::new(IpAddr::V4(Ipv4Addr::from(u32::from_be(saddr))), sport)
    }

    pub fn remote(&self) -> SocketAddr {
        let daddr = unsafe { (*self.ptr).ap.daddr };
        let dport = unsafe { (*self.ptr).ap.dport };
        SocketAddr::new(IpAddr::V4(Ipv4Addr::from(u32::from_be(daddr))), dport)
    }

    pub fn addr_pair(&self) -> (SocketAddr, SocketAddr) {
        (self.local(), self.remote())
    }

    pub fn protocol(&self) -> ProtocolType {
        ProtocolType::from(unsafe { *self.ptr }.protocol as i32)
    }

    pub fn state(&self) -> u8 {
        unsafe { (*self.ptr).state }
    }
    // latency module parameters
    pub fn queue_ts(&self) -> u64 {
        unsafe { (*self.ptr).__bindgen_anon_1.__bindgen_anon_1.queue_ts }
    }
    pub fn rcv_ts(&self) -> u64 {
        unsafe { (*self.ptr).__bindgen_anon_1.__bindgen_anon_1.rcv_ts }
    }
    pub fn pidtime_array_idx(&self) -> u32 {
        unsafe {
            (*self.ptr)
                .__bindgen_anon_1
                .__bindgen_anon_1
                .pidtime_array_idx
        }
    }
    pub fn socktime_array_idx(&self) -> u32 {
        unsafe {
            (*self.ptr)
                .__bindgen_anon_1
                .__bindgen_anon_1
                .socktime_array_idx
        }
    }
    // drop module parameters
    pub fn sk_protocol(&self) -> ProtocolType {
        ProtocolType::from(unsafe { (*self.ptr).__bindgen_anon_1.drop_params.sk_protocol as i32 })
    }

    pub fn skap(&self) -> (SocketAddr, SocketAddr) {
        let saddr = unsafe { (*self.ptr).__bindgen_anon_1.drop_params.skap.saddr };
        let sport = unsafe { (*self.ptr).__bindgen_anon_1.drop_params.skap.sport };
        let src = SocketAddr::new(IpAddr::V4(Ipv4Addr::from(u32::from_be(saddr))), sport);
        let daddr = unsafe { (*self.ptr).__bindgen_anon_1.drop_params.skap.daddr };
        let dport = unsafe { (*self.ptr).__bindgen_anon_1.drop_params.skap.dport };
        let dst = SocketAddr::new(IpAddr::V4(Ipv4Addr::from(u32::from_be(daddr))), dport);
        (src, dst)
    }

    pub fn stackid(&self) -> u32 {
        unsafe { (*self.ptr).stackid }
    }

    pub fn iptables_name(&self) -> String {
        unsafe {
            String::from_utf8_unchecked((*self.ptr).__bindgen_anon_1.drop_params.name.to_vec())
        }
    }

    pub fn iptables_chain_name(&self) -> String {
        let hooknum = unsafe { (*self.ptr).__bindgen_anon_1.drop_params.hook };
        match hooknum {
            0 => "PREROUTING".to_owned(),
            1 => "LOCAL_IN".to_owned(),
            2 => "FORWARD".to_owned(),
            3 => "LOCAL_OUT".to_owned(),
            4 => "POSTROUTING".to_owned(),
            _ => "unknown".to_owned(),
        }
    }
    // abnormal module params
    pub fn accept_queue(&self) -> u32 {
        unsafe { (*self.ptr).__bindgen_anon_1.abnormal.sk_ack_backlog }
    }

    pub fn syn_queue(&self) -> u32 {
        unsafe { (*self.ptr).__bindgen_anon_1.abnormal.icsk_accept_queue }
    }

    pub fn rcv_mem(&self) -> u32 {
        unsafe { (*self.ptr).__bindgen_anon_1.abnormal.rmem_alloc }
    }

    pub fn snd_mem(&self) -> u32 {
        unsafe { (*self.ptr).__bindgen_anon_1.abnormal.sk_wmem_queued }
    }

    pub fn drop(&self) -> u32 {
        unsafe { (*self.ptr).__bindgen_anon_1.abnormal.drop }
    }

    pub fn retran(&self) -> u32 {
        unsafe { (*self.ptr).__bindgen_anon_1.abnormal.retran }
    }

    pub fn ooo(&self) -> u32 {
        unsafe { (*self.ptr).__bindgen_anon_1.abnormal.ooo }
    }

    pub fn inum(&self) -> u32 {
        unsafe { (*self.ptr).__bindgen_anon_1.abnormal.i_ino }
    }

    pub fn set_abnormal_score(&mut self, score: u32) {
        self.score = Some(score);
    }

    pub fn abnormal_score(&self) -> u32 {
        self.score.unwrap()
    }
   
}

pub struct FourSecondsRing {
    fsr: seconds4_ring,
}

fn onesecond_bit_exist(os: &onesecond, pos: u64) -> bool {
    let idx = pos / 32;
    let bit = pos & 0x1f;

    if (os.clear & (1 << idx)) != 0 {
        return (os.bitmap[idx as usize] & (1 << bit)) != 0;
    }

    false
}

pub fn tss_in_range(fsr: *const seconds4_ring, left: u64, right: u64) -> Vec<u64> {
    let mut tss = Vec::new();
    unsafe {
        for os in (*fsr).os {
            let mut startpos = 0;
            let mut endpos = 0;
            if os.ts <= left {
                startpos = (left - os.ts) / 1_000_000;
            }

            if os.ts <= right {
                endpos = std::cmp::min(1000, (right - os.ts) / 1_000_000 + 1);
            }

            if startpos >= endpos {
                continue;
            }

            for pos in startpos..endpos {
                if onesecond_bit_exist(&os, pos) {
                    tss.push(pos * 1_000_000 + os.ts);
                }
            }
        }
    }
    tss.sort();
    tss
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
