use crate::common::*;
use crate::utils::*;
use crate::Command;
use anyhow::{bail, Result};
use eutils_rs::proc::{Kallsyms, Netstat};
use eutils_rs::{net::ProtocolType, net::TcpState, proc::Snmp};
use std::collections::HashMap;
use std::fmt;
use structopt::StructOpt;

#[derive(Debug, StructOpt)]
pub struct RetranCommand {
    #[structopt(long, help = "Process identifier of container")]
    pid: Option<usize>,
    #[structopt(long, help = "Local network address of traced sock")]
    src: Option<String>,
    #[structopt(long, help = "Remote network address of traced sock")]
    dst: Option<String>,
}

pub fn build_retran(opts: &RetranCommand, debug: bool, btf: &Option<String>) -> Result<()> {
    bail!("not support yet")
}
