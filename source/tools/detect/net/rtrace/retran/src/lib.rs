mod bindings {
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}

mod skel {
    include!(concat!(env!("OUT_DIR"), "/retran.skel.rs"));
}
mod retran;

use bindings::retran_event;
use eutils_rs::net::TcpState;
use std::net::{IpAddr, Ipv4Addr, SocketAddr};
pub use retran::Retran;

pub struct RetranFilter {

}

use serde::{Deserialize, Serialize};


#[derive(Serialize, Deserialize)]
pub struct RetranEvent {
    pub tcp_state: String,
    pub ca_state: String,
    pub retran_type: String,
    pub sport: u16,
    pub dport: u16,
    pub sip: String,
    pub dip: String,
    pub ts: u64,
}


impl RetranEvent {
    pub fn from_event(event: &retran_event) -> Self {
        let tcp_state = TcpState::from(event.tcp_state as i32).to_string();
        let mut ca_state = "".to_owned();
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
        let retran_type = match event.retran_type {
            0 => "SynRetran",
            1 => "SlowStartRetran",
            2 => "RtoRetran",
            3 => "FastRetran",
            4 => "TLP",
            _ => "Other",
        };
        RetranEvent {
            tcp_state,
            ca_state,
            ts: event.ts,
            retran_type: retran_type.to_owned(),

            sport: event.ap.sport,
            dport: event.ap.dport,
            sip: IpAddr::V4(Ipv4Addr::from(u32::from_be(event.ap.saddr))).to_string(),
            dip: IpAddr::V4(Ipv4Addr::from(u32::from_be(event.ap.daddr))).to_string(),
        }
    }
}

