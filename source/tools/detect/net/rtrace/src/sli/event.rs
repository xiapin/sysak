use crate::sli_bindings::*;
use anyhow::{bail, Result};

#[derive(Default)]
pub struct LatencyHist {
    overflow: u32,
    latency: [u32; MAX_LATENCY_SLOTS as usize],
}

impl LatencyHist {
    pub fn new(ptr: *const latency_hist) -> LatencyHist {
        let mut lh = LatencyHist::default();
        unsafe {
            for (i, j) in (*ptr).latency.into_iter().enumerate() {
                lh.latency[i] = j;
            }

            lh.overflow = (*ptr).overflow;
        }
        lh
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
    pub fn from_event(ptr: *const event) -> Result<Event> {
        let et = unsafe { (*ptr).event_type } as u32;
        match et {
            LATENCY_EVENT => Ok(Event::LatencyEvent(LatencyEvent::New(unsafe {
                (*ptr).__bindgen_anon_1.le
            }))),
            _ => {
                bail!("Can't recognize event type: {}", et)
            }
        }
    }
}
