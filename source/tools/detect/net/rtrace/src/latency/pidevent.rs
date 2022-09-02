use crate::latency::skel::*;
use std::fmt;

#[derive(Clone, Copy)]
pub enum PidEventType {
    SchedIn,
    SchedOut,
    Unknow,
}

impl From<u32> for PidEventType {
    fn from(value: u32) -> Self {
        match value {
            PID_EVENTS_SCHED_IN => PidEventType::SchedIn,
            PID_EVENTS_SCHED_OUT => PidEventType::SchedOut,
            _ => PidEventType::Unknow,
        }
    }
}

impl fmt::Display for PidEventType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let str;
        match &self {
            PidEventType::SchedIn => str = "SchedIn",
            PidEventType::SchedOut => str = "SchedOut",
            PidEventType::Unknow => str = "Unknow",
        }
        write!(f, "{}", str)
    }
}
