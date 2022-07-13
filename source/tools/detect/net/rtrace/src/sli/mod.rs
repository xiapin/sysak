use eutils_rs::proc::Snmp;
mod sli;
use anyhow::{bail, Result};
use std::{thread, time};
use structopt::StructOpt;

#[derive(Debug, StructOpt)]
pub struct SliCommand {
    #[structopt(long, help = "Collect retransmission metrics")]
    retran: bool,
    #[structopt(long, default_value = "3", help = "Data collection cycle, in seconds")]
    period: u64,
}

pub struct SliOutput {
    // retranmission metrics
    // retran = (RetransSegs－last RetransSegs) ／ (OutSegs－last OutSegs) * 100%
    outsegs: u32,   // Tcp: OutSegs
    retran: u32,    // Tcp: RetransSegs

    drop: u32,

    latency: [u32;1000],
    
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

pub fn build_sli(opts: &SliCommand) -> Result<()> {
    let mut old_snmp = Snmp::from_file("/proc/net/snmp")?;
    let sleep = time::Duration::from_secs(opts.period);

    loop {
        thread::sleep(sleep);
        let new_snmp = Snmp::from_file("/proc/net/snmp")?;

        if opts.retran {
            println!(
                "OutSegs: {}, Retran: {}",
                snmp_delta(&old_snmp, &new_snmp, ("Tcp:", "OutSegs"))?,
                snmp_delta(&old_snmp, &new_snmp, ("Tcp:", "RetransSegs"))?,
            );
        }

        old_snmp = new_snmp;
    }
}
