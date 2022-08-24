// mod abnormal;
mod drop;
mod latency;
mod latencylegacy;
// mod sli;
mod utils;
mod stack;

mod common;
mod perf;

use anyhow::{bail, Result};
use structopt::StructOpt;

use drop::{DropCommand, build_drop};
use latency::latency::{LatencyCommand, build_latency};


// use abnormal::build_abnormal;
// use latency::{build_latency, LatencyCommand};
use latencylegacy::{build_latency_legacy, LatencyLegacyCommand};
// use sli::build_sli;

// use abnormal::AbnormalCommand;
// use sli::SliCommand;

#[derive(Debug, StructOpt)]
#[structopt(name = "rtrace", about = "Diagnosing tools of kernel network")]
pub struct Command {
    #[structopt(subcommand)]
    subcommand: SubCommand,

    #[structopt(long, help = "Custom btf path")]
    btf: Option<String>,

    #[structopt(short, long, help = "Verbose debug output")]
    verbose: bool,
}

#[derive(Debug, StructOpt)]
enum SubCommand {
    // #[structopt(name = "abnormal", about = "Abnormal connection diagnosing")]
    // Abnormal(AbnormalCommand),
    #[structopt(name = "drop", about = "Packet drop diagnosing")]
    Drop(DropCommand),
    #[structopt(name = "latency", about = "Packet latency tracing")]
    Latency(LatencyCommand),
    #[structopt(name = "latencylegacy", about = "Packet latency tracing(legacy version)")]
    LatencyLegacy(LatencyLegacyCommand),
    // #[structopt(name = "sli", about = "Collection machine sli")]
    // Sli(SliCommand),
}

fn main() -> Result<()> {
    env_logger::init();
    let opts = Command::from_args();

    eutils_rs::helpers::bump_memlock_rlimit()?;

    match opts.subcommand {
        // SubCommand::Abnormal(cmd) => {
        //     build_abnormal(&cmd)?;
        // }
        SubCommand::Drop(cmd) => {
            build_drop(&cmd, opts.verbose, &opts.btf)?;
        }
        SubCommand::Latency(cmd) => {
            build_latency(&cmd, opts.verbose, &opts.btf)?;
        }
        SubCommand::LatencyLegacy(cmd) => {
            build_latency_legacy(&cmd, opts.verbose, &opts.btf)?;
        }
        // SubCommand::Sli(cmd) => {
        //     build_sli(&cmd)?;
        // }
    }

    Ok(())
}
