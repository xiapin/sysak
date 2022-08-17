#[path = "bindings/commonbinding.rs"]
pub mod commonbinding;

use commonbinding::*;
use std::net::{IpAddr, Ipv4Addr, SocketAddr, SocketAddrV4};
use anyhow::{bail, Result};

/// Filter
pub struct Filter {
    filter: filter,
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

    pub fn set_ap(&mut self, ap: addr_pair) {
        self.filter.ap = ap;
    }

    pub fn vec(&self) -> Vec<u8> {
        unsafe {
            std::slice::from_raw_parts(
                &self.filter as *const filter as *const u8,
                std::mem::size_of::<filter>(),
            )
            .to_vec()
        }
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
