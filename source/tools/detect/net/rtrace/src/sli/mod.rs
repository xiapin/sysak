
#[path = "bpf/.output/skel.rs"]
mod skel;
use skel::Skel;

use eutils_rs::proc::Snmp;
mod sli;
use anyhow::{bail, Result};
use event::Event;
use event::LatencyHist;
use sli::Sli;
use std::{thread, time};

pub struct SliOutput {
    // retranmission metrics
    // retran = (RetransSegs－last RetransSegs) ／ (OutSegs－last OutSegs) * 100%
    outsegs: isize, // Tcp: OutSegs
    retran: isize,  // Tcp: RetransSegs

    drop: u32,

    latencyhist: LatencyHist,

    events: Vec<Event>,
}

fn snmp_delta(old: &Snmp, new: &Snmp, key: (&str, &str)) -> Result<isize> {
    let lookupkey = (key.0.to_owned(), key.1.to_owned());
    let val1 = old.lookup(&lookupkey);
    let val2 = new.lookup(&lookupkey);

    if let Some(x) = val1 {
        if let Some(y) = val2 {
            return Ok(y - x);
        }
    }

    bail!("failed to find key: {:?}", key)
}

fn latency_sli(sli: &mut Sli) -> Result<()> {
    Ok(())
}

