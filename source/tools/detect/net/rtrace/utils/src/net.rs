use cenum_rs::CEnum;

#[derive(Debug, CEnum)]
#[cenum(i32)]
pub enum ProtocolType {
    #[cenum(value = "libc::IPPROTO_ICMP", display = "icmp")]
    Icmp,
    #[cenum(value = "libc::IPPROTO_TCP", display = "tcp")]
    Tcp,
    #[cenum(value = "libc::IPPROTO_UDP", display = "udp")]
    Udp,
}

#[derive(Debug, CEnum)]
#[cenum(i32)]
pub enum CongestionState {
    #[cenum(value = "0", display = "Open")]
    Open,
    #[cenum(value = "1", display = "Disorder")]
    Disorder,
    #[cenum(value = "2", display = "CWR")]
    Cwr,
    #[cenum(value = "3", display = "Recovery")]
    Recovery,
    #[cenum(value = "4", display = "Loss")]
    Loss,
}

use anyhow::{bail, Result};
use bincode::{config, Decode, Encode};
use std::fmt::Display;
use std::fs::read_to_string;
use std::path::Path;
use std::str::FromStr;
use std::{collections::HashMap, ops::Add};

#[derive(Default, Debug, Clone, Encode, Decode, PartialEq)]
struct Netstat {
    hm: HashMap<(String, String), isize>,
}

impl FromStr for Netstat {
    type Err = anyhow::Error;
    fn from_str(content: &str) -> Result<Self> {
        let mut netstat = Netstat::default();

        let lines = content.split('\n').collect::<Vec<&str>>();

        for i in 0..lines.len() / 2 {
            let line1 = lines[i * 2];
            let line2 = lines[i * 2 + 1];

            let mut iter1 = line1.split_whitespace();
            let mut iter2 = line2.split_whitespace();

            let prefix;
            if let Some(x) = iter1.next() {
                prefix = x.to_string();
            } else {
                bail!("failed to parse: prefix not found")
            }
            iter2.next();
            loop {
                let k;
                let v: isize;
                if let Some(x) = iter1.next() {
                    k = x;
                } else {
                    break;
                }

                if let Some(x) = iter2.next() {
                    v = x.parse()?;
                } else {
                    bail!("failed to parse: number of item is not match.")
                }

                netstat.insert((prefix.clone(), k.to_string()), v);
            }
        }

        Ok(netstat)
    }
}

impl Netstat {
    pub fn from_file<P>(path: P) -> Result<Netstat>
    where
        P: AsRef<Path>,
    {
        let string = read_to_string(path)?;
        Netstat::from_str(&string)
    }

    pub(crate) fn insert(&mut self, k: (String, String), v: isize) {
        self.hm.insert(k, v);
    }
}

use std::net::IpAddr;
use std::net::Ipv4Addr;
use std::net::SocketAddr;

pub struct Addrpair {
    local: SocketAddr,
    remote: SocketAddr,
}

impl Default for Addrpair {
    fn default() -> Self {
        Addrpair {
            local: SocketAddr::new(IpAddr::V4(Ipv4Addr::new(0, 0, 0, 0)), 0),
            remote: SocketAddr::new(IpAddr::V4(Ipv4Addr::new(0, 0, 0, 0)), 0),
        }
    }
}

impl Display for Addrpair {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} -> {}", self.local, self.remote)
    }
}

#[macro_export]
macro_rules! addr_pair_2_Addrpair {
    ($ap: expr) => {
        Addrpair::new($ap.saddr, $ap.sport, $ap.daddr, $ap.dport)
    };
}

impl Addrpair {
    pub fn new(saddr: u32, sport: u16, daddr: u32, dport: u16) -> Self {
        Addrpair {
            local: SocketAddr::new(IpAddr::V4(Ipv4Addr::from(u32::from_be(saddr))), sport),
            remote: SocketAddr::new(IpAddr::V4(Ipv4Addr::from(u32::from_be(daddr))), dport),
        }
    }

    // pub fn from_string() -> Self {

    // }
}

pub fn parse_protocol(proto: &str) -> Result<u16> {
    let mut protocol = 0;
    match proto {
        "all" => protocol = 0,
        "tcp" => protocol = libc::IPPROTO_TCP,
        "udp" => protocol = libc::IPPROTO_UDP,
        "icmp" => protocol = libc::IPPROTO_ICMP,
        _ => bail!("failed to parse protocol: {}", protocol),
    }
    Ok(protocol as u16)
}
