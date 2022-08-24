


#[path = "bpf/.output/skel.rs"]
mod skel;
use skel::*;
pub mod latency;
mod pidevent;
mod sockevent;
mod event;