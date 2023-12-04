use serde::Deserialize;
use serde::Serialize;
use std::fmt;

#[derive(Debug, Clone, Copy, Eq, PartialEq, PartialOrd, Ord, Deserialize, Serialize)]
#[repr(i32)]
pub enum Protocol {
    Icmp = libc::IPPROTO_ICMP,
    Tcp = libc::IPPROTO_TCP,
    Udp = libc::IPPROTO_UDP,
}

impl Default for Protocol {
    fn default() -> Self {
        Protocol::Tcp
    }
}

impl TryFrom<&str> for Protocol {
    type Error = &'static str;
    fn try_from(value: &str) -> Result<Self, Self::Error> {
        match value {
            "icmp" => Ok(Protocol::Icmp),
            "tcp" => Ok(Protocol::Tcp),
            "udp" => Ok(Protocol::Udp),
            _ => Err("Unknown protocol string"),
        }
    }
}

impl TryFrom<i32> for Protocol {
    type Error = &'static str;
    fn try_from(value: i32) -> Result<Self, Self::Error> {
        match value {
            libc::IPPROTO_ICMP => Ok(Protocol::Icmp),
            libc::IPPROTO_TCP => Ok(Protocol::Tcp),
            libc::IPPROTO_UDP => Ok(Protocol::Udp),
            _ => Err("unsupport protocol type"),
        }
    }
}

impl fmt::Display for Protocol {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Protocol::Icmp => write!(f, "icmp"),
            Protocol::Tcp => write!(f, "tcp"),
            Protocol::Udp => write!(f, "udp"),
        }
    }
}
