mod bindings {
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}

mod skel {
    include!(concat!(env!("OUT_DIR"), "/drop.skel.rs"));
}

mod drop;
// pub mod json;

pub use drop::Drop;
use eutils_rs::net::{ProtocolType, TcpState};
use std::fmt;
use std::net::SocketAddr;
use cenum_rs::CEnum;
use crate::bindings::*;
use anyhow::Result;
use std::net::SocketAddrV4;
use utils::net::Addrpair;

// #[derive(CEnum)]
// pub enum DropType {
//     #[cenum(value = "NF_CONNTRACK", display = "ConntrackDrop")]
//     NFConntrack,
//     #[cenum(value = "IPTABLES", display = "IptablesDrop")]
//     Iptables,
//     #[cenum(value = "KFREE_SKB", display = "KfreeSkb")]
//     KfreeSkb,
//     #[cenum(value = "TCP_DROP", display = "TcpDrop")]
//     TcpDrop,
// }


pub struct DropFilter {
    filter: drop_filter,
}

impl DropFilter {

    pub fn new() -> Self {
        DropFilter {
            filter: utils::init_zeroed!()
        }
    }
    pub fn set_protocol(&mut self, protocol: u16) {
        self.filter.protocol = protocol;
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
    
    // pub(crate) fn set_pid(&mut self, pid: u32) {
    //     self.filter.pid = pid;
    // }

    pub fn to_vec(&self) -> Vec<u8> {
        utils::to_vecu8!(&self.filter, drop_filter)
    }
}

use utils::kernel_stack::KernelStack;

pub struct DropEvent {
    ty: u8,
    cpu: u16,
    has_sk: bool,

    sk_proto: ProtocolType,
    sk_state: TcpState,
    skap: Addrpair,
    skb_proto: ProtocolType,
    skbap: Addrpair,
    table: String,
    chain: String,
    stack: Option<KernelStack>,
}

impl DropEvent {
    pub fn from_drop_event(de: drop_event) -> Self{
        DropEvent {
            ty: de.type_,
            cpu: de.cpu,
            has_sk: de.has_sk != 0,
            sk_proto: ProtocolType::from(de.sk_protocol as i32),
            sk_state: TcpState::from(de.sk_state as i32),
            skap: utils::addr_pair_2_Addrpair!(de.skap),
            skb_proto: ProtocolType::from(de.skb_protocol as i32),
            skbap: utils::addr_pair_2_Addrpair!(de.skbap),
            table: unsafe { String::from_utf8_unchecked(de.name.to_vec()) },
            chain: match de.hook {
                0 => "PREROUTING".to_owned(),
            1 => "LOCAL_IN".to_owned(),
            2 => "FORWARD".to_owned(),
            3 => "LOCAL_OUT".to_owned(),
            4 => "POSTROUTING".to_owned(),
            _ => "unknown".to_owned(),
            },
            stack: None,
        }
    }

    pub fn set_stack(&mut self, stack: Vec<u8>) {
        self.stack = Some(KernelStack::new(&stack));
    }
}

impl fmt::Display for DropEvent {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Sock Info:")?;
        if self.has_sk {
            match self.sk_proto {
                ProtocolType::Tcp => {
                    writeln!(
                        f,
                        "protocol: {}  {} state: {}",
                        self.sk_proto, self.skap, self.sk_state
                    )?;
                }
                ProtocolType::Unknown(x) => {
                    writeln!(f, "Unkown network protocol: {}", x)?;
                }
                _ => {
                    writeln!(
                        f,
                        "protocol: {}  {}",
                        self.sk_proto, self.skap
                    )?;
                }
            }
        } else {
            writeln!(f, "Sock of skb not found")?;
        }
        write!(f, "Skb Info :")?;
        match self.skb_proto {
            ProtocolType::Tcp | ProtocolType::Udp | ProtocolType::Icmp => {
                writeln!(
                    f,
                    "protocol: {} {}",
                    self.skb_proto, self.skbap
                )?;
            }
            _ => {
                writeln!(f, "Unknow network protocol")?;
            }
        }

        match self.ty as u32 {
            IPTABLES => {
                writeln!(f, "IptablesDrop : Iptables-Table: {} Iptables-Chain: {}", self.table, self.chain)?;
            }
            NF_CONNTRACK => {
                writeln!(f, "NfconntrackDrop")?;
            }
            _ => {
                if let Some(stack) = &self.stack {
                        write!(f, "{}", stack)?;
                } else {
                    write!(f, "failed to get stack")?;
                }
                
            }
        }

        

        Ok(())
    }
}
