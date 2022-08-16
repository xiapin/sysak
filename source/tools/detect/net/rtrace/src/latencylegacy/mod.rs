pub mod icmp;
pub mod tcp;

use self::icmp::build_icmp;
use self::tcp::build_tcp;
use anyhow::{bail, Result};
use structopt::StructOpt;

#[derive(Debug, StructOpt)]
pub struct LatencyCommand {
    #[structopt(long, default_value = "tcp", help = "Network protocol type")]
    proto: String,
    #[structopt(long, help = "Process identifier")]
    pid: Option<usize>,
    #[structopt(long, default_value = "0", help = "The threshold of latency")]
    latency: u64,
    #[structopt(long, help = "Local network address of traced sock")]
    src: Option<String>,
    #[structopt(long, help = "Remote network address of traced sock")]
    dst: Option<String>,
}

pub fn build_latency(opts: &LatencyCommand) -> Result<()> {
    match opts.proto.as_str() {
        "tcp" => build_tcp(opts)?,
        "icmp" => build_icmp(opts)?,
        _ => bail!("Only support icmp and tcp protocol"),
    }

    Ok(())
}
