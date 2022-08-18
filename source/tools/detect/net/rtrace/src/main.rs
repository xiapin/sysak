mod abnormal;
mod drop;
mod latency;
mod sli;
mod utils;

mod common;
mod perf;

use abnormal::build_abnormal;
use anyhow::{bail, Result};
use drop::build_drop;
use latency::{build_latency, LatencyCommand};
use sli::build_sli;
use structopt::StructOpt;

use abnormal::AbnormalCommand;
use drop::DropCommand;
use sli::SliCommand;

#[derive(Debug, StructOpt)]
#[structopt(name = "rtrace", about = "Diagnosing tools of kernel network")]
pub struct Command {
    #[structopt(subcommand)]
    subcommand: SubCommand,
}

#[derive(Debug, StructOpt)]
enum SubCommand {
    #[structopt(name = "abnormal", about = "Abnormal connection diagnosing")]
    Abnormal(AbnormalCommand),
    #[structopt(name = "drop", about = "Packet drop diagnosing")]
    Drop(DropCommand),
    Latency(LatencyCommand),
    #[structopt(name = "sli", about = "Collection machine sli")]
    Sli(SliCommand),
}

fn main() -> Result<()> {
    env_logger::init();
    let opts = Command::from_args();

    match opts.subcommand {
        SubCommand::Abnormal(cmd) => {
            build_abnormal(&cmd)?;
        }
        SubCommand::Drop(cmd) => {
            build_drop(&cmd)?;
        }
        SubCommand::Latency(cmd) => {
            build_latency(&cmd)?;
        }
        SubCommand::Sli(cmd) => {
            build_sli(&cmd)?;
        }
    }

    Ok(())
}
