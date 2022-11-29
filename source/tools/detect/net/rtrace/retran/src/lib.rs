mod bindings {
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}

mod skel {
    include!(concat!(env!("OUT_DIR"), "/retran.skel.rs"));
}
mod retran;

use bindings::retran_event;
use eutils_rs::net::TcpState;
use std::net::{IpAddr, Ipv4Addr, SocketAddr, SocketAddrV4};
pub use retran::Retran;

pub struct RetranFilter {

}

use serde::{Deserialize, Serialize};


#[derive(Serialize, Deserialize)]
pub struct RetranEvent {
    pub ap:String,
    pub tcp_state: String,
    pub ca_state: String,
    pub times: usize,
    
    pub ts: u64,
}


impl RetranEvent {
    pub fn from_event(event: &retran_event) -> Self {
        let tcp_state = TcpState::from(event.tcp_state as i32).to_string();
        let mut ca_state = "".to_owned();
        let src = SocketAddr::new(
            IpAddr::V4(Ipv4Addr::from(u32::from_be(event.ap.saddr))),
            event.ap.sport,
        );
        let dst = SocketAddr::new(
            IpAddr::V4(Ipv4Addr::from(u32::from_be(event.ap.daddr))),
            event.ap.dport,
        );
        match event.ca_state {
            0 => {
                ca_state = "open".to_owned();
            }
            1 => {
                ca_state = "disorder".to_owned();
            }
            2 => {
                ca_state = "cwr".to_owned();
            }
            3 => {
                ca_state = "recovery".to_owned();
            }
            4 => {
                ca_state = "loss".to_owned();
            }
            _ => {
                ca_state = "none".to_owned();
            }
        }
        RetranEvent {
            tcp_state,
            ca_state,
            ap: format!("{} -> {}", src.to_string(), dst.to_string()),
            times: event.retran_times as usize,
            ts: event.ts,
        }
    }
}

