

#[path = "bpf/.output/latency.skel.rs"]
pub mod skel;


mod latency;

use structopt::StructOpt;
use anyhow::{bail, Result};

#[derive(Debug, StructOpt)]
pub struct LatencyCommand {
    #[structopt(long, help = "Custom btf path")]
    btf: Option<String>,
    #[structopt(long, help = "Local network address of traced skb")]
    src: Option<String>,
    #[structopt(long, help = "Remote network address of traced skb")]
    dst: Option<String>,

    #[structopt(
        long,
        default_value = "3",
        help = "Period of display in seconds."
    )]
    period: u64,
}

pub fn build_latency(opts: &LatencyCommand) -> Result<()> {

    Ok(())
}