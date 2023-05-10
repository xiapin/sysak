mod bindings {
    include!("bpf/bindings.rs");
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


use utils::kernel_stack::{KernelStack, GLOBAL_KALLSYMS};

use serde::{Deserialize, Serialize};


#[derive(Serialize, Deserialize)]
pub struct DropEvent {
    proto: u16,
    src: String,
    dst: String,
    symbol: String,
}

impl DropEvent {
    pub fn from_drop_event(de: drop_event) -> Self{
        let ap = utils::addr_pair_2_Addrpair!(de.ap);
        DropEvent {
            src: ap.local.to_string(),
            dst: ap.remote.to_string(),
            proto: de.proto,
            symbol: GLOBAL_KALLSYMS.lock().unwrap().addr_to_sym(de.location),
        }
    }
}
