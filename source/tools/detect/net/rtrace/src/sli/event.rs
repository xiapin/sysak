use crate::sli_bindings::*;
use anyhow::{bail, Result};

pub struct LatencyHist {
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
                overflow: (*ptr).overflow,
                latency,
            }
        }
    }
}

pub struct LatencyEvent {
    latency: u32,
}

impl LatencyEvent {
    pub fn new(ptr: *const latency_event) -> LatencyEvent {
        LatencyEvent {
            latency: unsafe { (*ptr).latency },
        }
    }
}

pub enum Event {
    LatencyEvent(LatencyEvent),
}

impl Event {
    pub fn new(ptr: *const event) -> Result<Event> {
        let et = unsafe { (*ptr).event_type } as u32;
        match et {
            LATENCY_EVENT => Ok(Event::LatencyEvent(LatencyEvent::new(unsafe {
                &(*ptr).__bindgen_anon_1.le
            }))),
            _ => {
                bail!("Can't recognize event type: {}", et)
            }
        }
    }
}
