use crate::common::protocol::Protocol;
use std::time::Duration;

#[derive(Debug, Clone, Default)]
pub struct Config {
    pub threshold: u64,
    pub src: (u32, u16),
    pub dst: (u32, u16),
    pub protocol: Protocol,
    pub jitter: bool,
    pub drop: bool,
    pub retran: bool,
    pub verbose: bool,
    pub ping: bool,

    pub output_raw: bool,
    pub output_json: bool,
    pub interface: String,
    pub period: Duration,
    pub virtio: bool,

    pub disable_kfree_skb: bool,
    pub tcpping: bool,
    pub count: u32,
    pub iqr: bool,
}

impl Config {
    pub fn set_protocol_icmp(&mut self) {
        self.protocol = Protocol::Icmp;
    }

    pub fn set_protocol_tcp(&mut self) {
        self.protocol = Protocol::Tcp;
    }

    pub fn enable_drop(&mut self) {
        self.drop = true;
    }

    pub fn enable_retran(&mut self) {
        self.retran = true;
    }

    pub fn disable_drop_kfree_skb(&mut self) {
        self.disable_kfree_skb = true;
    }

    pub fn enable_virtio(&self) -> bool {
        self.virtio
    }
}
