
use crate::latency::skel::*;
use std::fmt;

#[derive(Clone, Copy)]
pub enum SockEventType {
    SockDefReadable,
    Unkonw,
}

impl From<u32> for SockEventType {
    fn from(value: u32) -> Self {
        match value {
            SOCK_EVENTS_SOCK_DEF_READABLE => SockEventType::SockDefReadable,
            _ => SockEventType::Unkonw,
        }
    }
}

impl fmt::Display for SockEventType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let str;
        match &self {
            SockEventType::SockDefReadable => str = "PollNotify",
            SockEventType::Unkonw => str = "Unknow",
        }
        write!(f, "{}", str)
    }
}