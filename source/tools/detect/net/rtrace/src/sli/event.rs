use crate::sli::bindings::*;
use anyhow::{bail, Result};
use std::fmt;
use std::net::{IpAddr, Ipv4Addr, SocketAddr, SocketAddrV4};

pub struct LatencyHist {
    threshold: u32,
    overflow: u32,
    latency: [u32; MAX_LATENCY_SLOTS as usize],
}

impl LatencyHist {
    pub fn new(ptr: *const latency_hist) -> LatencyHist {
        unsafe {
            let mut latency = [0; MAX_LATENCY_SLOTS as usize];
            for (i, j) in (*ptr).latency.into_iter().enumerate() {
                latency[i] = j;
            }

            LatencyHist {
                threshold: (*ptr).threshold,
                overflow: (*ptr).overflow,
                latency,
            }
        }
    }
}

impl fmt::Display for LatencyHist {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let mut cnt = 0;
        let mut interval = ((self.threshold + 15) / 16) as usize; // Up to 16 lines
        let mut new_latency = [0; 16]; // Up to 16 lines

        for (i, j) in self.latency.into_iter().enumerate() {
            if i >= self.threshold as usize {
                break;
            }
            cnt += j;
            new_latency[i / interval] += j;
        }

        if cnt == 0 {
            cnt = 1;
        }

        for (i, j) in new_latency.into_iter().enumerate() {
            let probablity = j * 100 / cnt;
            let start = i * interval;
            if start >= self.threshold as usize {
                break;
            }
            let end = std::cmp::min(self.threshold as usize, (i + 1) * interval);
            write!(
                f,
                "{:>4}-{:<4} | {:<100} {}\n",
                start,
                end,
                "*".repeat(probablity as usize),
                j
            )?;
        }
        write!(f, "execced {} ms times: {}", self.threshold, self.overflow)
    }
}

pub struct LatencyEvent {
    pid: u32,
    comm: String,
    src: SocketAddr,
    dst: SocketAddr,
    latency: u32,
}

impl LatencyEvent {
    pub fn new(ptr: *const latency_event) -> LatencyEvent {
        let pid = unsafe { (*ptr).pid };
        let comm = unsafe { String::from_utf8_unchecked((*ptr).comm.to_vec()) };
        let daddr = unsafe { (*ptr).ap.daddr };
        let dport = unsafe { (*ptr).ap.dport };
        let saddr = unsafe { (*ptr).ap.saddr };
        let sport = unsafe { (*ptr).ap.sport };
        let src = SocketAddr::new(IpAddr::V4(Ipv4Addr::from(u32::from_be(saddr))), sport);
        let dst = SocketAddr::new(IpAddr::V4(Ipv4Addr::from(u32::from_be(daddr))), dport);
        LatencyEvent {
            pid,
            comm,
            latency: unsafe { (*ptr).latency },
            src,
            dst,
        }
    }
}

impl fmt::Display for LatencyEvent {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        if self.pid != 0 {
            write!(f, "{}/{} ", self.pid, self.comm)?;
        }
        write!(
            f,
            "{} -> {}, latency: {}ms",
            self.src, self.dst, self.latency
        )
    }
}

pub struct DropEvent {
    pid: u32,
    comm: String,
    src: SocketAddr,
    dst: SocketAddr,
}

impl DropEvent {
    pub fn new(ptr: *const drop_event) -> DropEvent {
        let pid = unsafe { (*ptr).pid };
        let comm = unsafe { String::from_utf8_unchecked((*ptr).comm.to_vec()) };
        let daddr = unsafe { (*ptr).ap.daddr };
        let dport = unsafe { (*ptr).ap.dport };
        let saddr = unsafe { (*ptr).ap.saddr };
        let sport = unsafe { (*ptr).ap.sport };
        let src = SocketAddr::new(IpAddr::V4(Ipv4Addr::from(u32::from_be(saddr))), sport);
        let dst = SocketAddr::new(IpAddr::V4(Ipv4Addr::from(u32::from_be(daddr))), dport);

        DropEvent {
            pid,
            comm,
            src,
            dst,
        }
    }
}

impl fmt::Display for DropEvent {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        if self.pid != 0 {
            write!(f, "{}/{} ", self.pid, self.comm)?;
        }
        write!(f, "{} -> {}", self.src, self.dst)
    }
}

pub enum Event {
    LatencyEvent(LatencyEvent),
    AppLatencyEvent(LatencyEvent),
    DropEvent(DropEvent),
}

impl Event {
    pub fn new(ptr: *const event) -> Result<Event> {
        let et = unsafe { (*ptr).event_type } as u32;
        match et {
            LATENCY_EVENT => Ok(Event::LatencyEvent(LatencyEvent::new(unsafe {
                &(*ptr).__bindgen_anon_1.le
            }))),
            APP_LATENCY_EVENT => Ok(Event::AppLatencyEvent(LatencyEvent::new(unsafe {
                &(*ptr).__bindgen_anon_1.le
            }))),
            DROP_EVENT => Ok(Event::DropEvent(DropEvent::new(unsafe {
                &(*ptr).__bindgen_anon_1.de
            }))),
            _ => {
                bail!("Can't recognize event type: {}", et)
            }
        }
    }
}
