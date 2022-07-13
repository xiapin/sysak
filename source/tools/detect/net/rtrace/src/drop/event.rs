use crate::drop_bindings::event;
use std::net::{IpAddr, Ipv4Addr, SocketAddr, SocketAddrV4};
use std::fmt;

pub struct Event {
    data: Vec<u8>,
    ptr: *const event,
}

impl Event {
    pub fn new(data: Vec<u8>) -> Event {
        Event {
            ptr: &data[0] as *const u8 as *const event,
            data,
        }
    }

    pub fn stackid(&self) -> i32 {
        unsafe {(*self.ptr).stackid}
    }

    pub fn pid(&self) -> u32 {
        unsafe { (*self.ptr).pi.pid }
    }

    pub fn comm(&self) -> String {
        unsafe { String::from_utf8_unchecked((*self.ptr).pi.comm.to_vec()) }
    }

    pub fn ap(&self) -> (SocketAddr, SocketAddr) {
        let daddr = unsafe { (*self.ptr).ap.daddr };
        let dport = unsafe { (*self.ptr).ap.dport };
        let saddr = unsafe { (*self.ptr).ap.saddr };
        let sport = unsafe { (*self.ptr).ap.sport };
        let src = SocketAddr::new(IpAddr::V4(Ipv4Addr::from(u32::from_be(saddr))), sport);
        let dst = SocketAddr::new(IpAddr::V4(Ipv4Addr::from(u32::from_be(daddr))), dport);
        (src, dst)
    }

    pub fn skap(&self) -> (SocketAddr, SocketAddr) {
        let daddr = unsafe { (*self.ptr).skap.daddr };
        let dport = unsafe { (*self.ptr).skap.dport };
        let saddr = unsafe { (*self.ptr).skap.saddr };
        let sport = unsafe { (*self.ptr).skap.sport };
        let src = SocketAddr::new(IpAddr::V4(Ipv4Addr::from(u32::from_be(saddr))), sport);
        let dst = SocketAddr::new(IpAddr::V4(Ipv4Addr::from(u32::from_be(daddr))), dport);
        (src, dst)
    }

    pub fn state(&self) -> u8 {
        unsafe { (*self.ptr).state }
    }

    pub fn protocol(&self) -> u16 {
        unsafe { (*self.ptr).protocol }
    }

    pub fn sk_protocol(&self) -> u16 {
        unsafe { (*self.ptr).sk_protocol }
    }

    pub fn syn_qlen(&self) -> u32 {
        unsafe {(*self.ptr).__bindgen_anon_1.tp.syn_qlen}
    }

    pub fn max_len(&self) -> u32 {
        unsafe {(*self.ptr).__bindgen_anon_1.tp.max_len}
    }

    pub fn syncookies(&self) -> u8 {
        unsafe {(*self.ptr).__bindgen_anon_1.tp.syncookies}
    }

    pub fn acc_qlen(&self) -> u32 {
        unsafe {(*self.ptr).__bindgen_anon_1.tp.acc_qlen}
    }

}

impl fmt::Display for Event {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "")
    }
}