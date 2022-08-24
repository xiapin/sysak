

mod tcpconnect;
use anyhow::{bail, Result};
use libbpf_rs::PerfBufferBuilder;
use structopt::StructOpt;
use std::fmt;
use std::time::Duration;
use std::net::{IpAddr, Ipv4Addr, SocketAddr, SocketAddrV4};

mod perf;

#[derive(Debug, StructOpt)]
struct Command {
    /// Verbose debug output
    #[structopt(short, long)]
    verbose: bool,
}

fn main() -> Result<()>{
    Ok(())
}
