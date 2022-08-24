#[path = "bpf/.output/bindings.rs"]
#[allow(non_upper_case_globals)]
#[allow(non_camel_case_types)]
#[allow(non_snake_case)]
#[allow(dead_code)]
mod bindings;

#[path = "bpf/.output/icmp.skel.rs"]
mod icmpskel;

#[path = "bpf/.output/tcp.skel.rs"]
mod tcpskel;


pub mod icmp;
pub mod tcp;

use self::icmp::build_icmp;
use self::tcp::build_tcp;
use anyhow::{bail, Result};
use structopt::StructOpt;

#[derive(Debug, StructOpt)]
pub struct LatencyLegacyCommand {
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
    #[structopt(long, help = "Custom btf path")]
    btf: Option<String>,
}

pub fn build_latency_legacy(opts: &LatencyLegacyCommand, debug: bool, btf: &Option<String>) -> Result<()> {
    match opts.proto.as_str() {
        "tcp" => build_tcp(opts, debug, btf)?,
        "icmp" => build_icmp(opts, debug, btf)?,
        _ => bail!("Only support icmp and tcp protocol"),
    }

    Ok(())
}
