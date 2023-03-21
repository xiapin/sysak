

use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Default)]
pub struct LatencyEvent {
    sender: bool,
    id: u16,
    seq: u16,

    send_ms: u8,
    out_ms: u8,
    recv_ms: u8,
}


impl LatencyEvent {


    pub fn new() -> Self {
        Self::default()
    }

    pub fn set_sender(&mut self) {
        self.sender = true
    }

    pub fn clr_sender(&mut self) {
        self.sender = false
    }

    pub fn clr_sender(&mut self) {
        self.sender = false
    }

    pub fn clr_sender(&mut self) {
        self.sender = false
    }
}