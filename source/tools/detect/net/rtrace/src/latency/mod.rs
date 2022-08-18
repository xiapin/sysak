#[path = "bpf/.output/bindings.rs"]
#[allow(non_upper_case_globals)]
#[allow(non_camel_case_types)]
#[allow(non_snake_case)]
#[allow(dead_code)]
mod bindings;

#[path = "bpf/.output/latency.skel.rs"]
pub mod skel;

mod latency;

use anyhow::{bail, Result};
use latency::Latency;
use structopt::StructOpt;

#[derive(Debug, StructOpt)]
pub struct LatencyCommand {
    #[structopt(long, help = "Custom btf path")]
    btf: Option<String>,
    // #[structopt(long, help = "Local network address of traced skb")]
    // src: Option<String>,
    // #[structopt(long, help = "Remote network address of traced skb")]
    // dst: Option<String>,
    #[structopt(long, help = "entry point")]
    entry: Option<String>,
    #[structopt(long, help = "exit point")]
    exit: Option<String>,

    #[structopt(long, default_value = "3", help = "Period of display in seconds.")]
    period: u64,
}

pub fn build_latency(opts: &LatencyCommand) -> Result<()> {

    eutils_rs::helpers::bump_memlock_rlimit()?;
    let mut latency = Latency::new(log::log_enabled!(log::Level::Debug), &opts.btf)?;

    latency.attach()?;

    loop {
        std::thread::sleep(std::time::Duration::from_secs(opts.period));
        let ohist = latency.get_loghist()?;
        if let Some(hist) = ohist {
            let logdis = hist.to_logdistribution();
            println!("{}", logdis);
        }
    }
    Ok(())
}
