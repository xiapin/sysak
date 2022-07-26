mod event;

use eutils_rs::proc::Snmp;
mod sli;
use anyhow::{bail, Result};
use event::Event;
use event::LatencyHist;
use sli::Sli;
use std::{thread, time};
use structopt::StructOpt;

#[derive(Debug, StructOpt)]
pub struct SliCommand {
    #[structopt(long, help = "Collect retransmission metrics")]
    retran: bool,

    #[structopt(long, help = "Collect latency metrics")]
    latency: bool,
    #[structopt(
        short,
        long,
        help = "Collect latency between kernel and application in receiving side"
    )]
    applat: bool,
    #[structopt(
        long,
        default_value = "1000",
        help = "Max latency to trace, default is 1000ms"
    )]
    threshold: u32,

    #[structopt(long, default_value = "3", help = "Data collection cycle, in seconds")]
    period: u64,

    #[structopt(long, help = "Output every sli to shell")]
    shell: bool,
}

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

pub fn build_sli(opts: &SliCommand) -> Result<()> {
    let mut old_snmp = Snmp::from_file("/proc/net/snmp")?;
    let delta_ns = opts.period * 1_000_000_000;
    let mut sli = Sli::new(log::log_enabled!(log::Level::Debug), opts.threshold)?;
    let mut sli_output: SliOutput = unsafe { std::mem::MaybeUninit::zeroed().assume_init() };
    let mut pre_ts = 0;
    let mut has_output = false;

    if opts.latency {
        sli.attach_latency()?;
        sli.lookup_and_update_latency_map(0)?;
    }

    if opts.applatency {
        sli.attach_applatency()?;
        sli.lookup_and_update_latency_map(1)?;
    }

    loop {
        if let Some(event) = sli.poll(std::time::Duration::from_millis(100))? {
            // log::debug!("{}", event);
            sli_output.events.push(event);
        }

        let cur_ts = eutils_rs::timestamp::current_monotime();
        if cur_ts - pre_ts < delta_ns {
            continue;
        }

        pre_ts = cur_ts;

        let new_snmp = Snmp::from_file("/proc/net/snmp")?;

        has_output = false;

        if opts.retran {
            sli_output.outsegs = snmp_delta(&old_snmp, &new_snmp, ("Tcp:", "OutSegs"))?;
            sli_output.retran = snmp_delta(&old_snmp, &new_snmp, ("Tcp:", "RetransSegs"))?;

            if opts.shell {
                println!(
                    "OutSegs: {}, Retran: {}, %Retran: {}",
                    sli_output.outsegs,
                    sli_output.retran,
                    sli_output.retran as f64 / sli_output.outsegs as f64
                );
                has_output = true;
            }
        }

        if opts.latency {
            if let Some(x) = sli.lookup_and_update_latency_map(0)? {
                sli_output.latencyhist = x;
            }

            if opts.shell {
                println!("rtt histogram");
                println!("{}", sli_output.latencyhist);
                for event in &sli_output.events {
                    match event {
                        Event::LatencyEvent(le) => {
                            println!("{}", le);
                        }
                        _ => {}
                    }
                }
                has_output = true;
            }
        }

        if opts.applatency {
            if opts.shell {
                let mut first = true;
                for event in &sli_output.events {
                    match event {
                        Event::AppLatencyEvent(le) => {
                            if first {
                                has_output = true;
                                first = false;
                                println!("application latency event:");
                            }
                            println!("{}", le);
                        }
                        _ => {}
                    }
                }
            }
        }

        old_snmp = new_snmp;
        // clear
        sli_output = unsafe { std::mem::MaybeUninit::zeroed().assume_init() };
        if has_output {
            println!("\n\n");
        }
    }
}
