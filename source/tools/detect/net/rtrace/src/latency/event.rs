use crate::latency::skel::*;
use crate::latency::pidevent::PidEventType;
use crate::latency::sockevent::SockEventType;
use std::fmt;

#[derive(Clone, Copy)]
pub enum EventTypeTs {
    KernelRcv(u64),
    AppRcv(u64),
    PidEvent(PidEventType, u64),
    SockEvent(SockEventType, u64),
}

impl EventTypeTs {
    pub fn ts(&self) -> u64 {
        match &self {
            EventTypeTs::PidEvent(_, ts)
            | EventTypeTs::SockEvent(_, ts)
            | EventTypeTs::AppRcv(ts)
            | EventTypeTs::KernelRcv(ts) => *ts,
        }
    }
}

impl fmt::Display for EventTypeTs {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match &self {
            EventTypeTs::PidEvent(x, _) => {
                write!(f, "{}", x)
            }
            EventTypeTs::SockEvent(x, _) => {
                write!(f, "{}", x)
            }
            EventTypeTs::AppRcv(_) => {
                write!(f, "AppRcv")
            }
            EventTypeTs::KernelRcv(_) => {
                write!(f, "KernelRcv")
            }
        }
    }
}
