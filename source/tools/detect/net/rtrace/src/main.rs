

mod abnormal;
mod drop;
mod sli;

mod perf;
mod common;

use anyhow::{bail, Result};
use drop::build_drop;
use abnormal::build_abnormal;
use sli::build_sli;
use structopt::StructOpt;

use drop::DropCommand;
use abnormal::AbnormalCommand;
use sli::SliCommand;

#[derive(Debug, StructOpt)]
#[structopt(name = "rtrace", about = "Diagnosing tools of kernel network")]
pub struct Command {
    #[structopt(subcommand)]
    subcommand: SubCommand,
}

#[derive(Debug, StructOpt)]
enum SubCommand {
    #[structopt(name = "drop", about = "Packet drop diagnosing")]
    Drop(DropCommand),
    #[structopt(name = "abnormal", about = "Abnormal connection diagnosing")]
    Abnormal(AbnormalCommand),
    #[structopt(name = "sli", about = "Collection machine sli")]
    Sli(SliCommand),
}

fn main() -> Result<()> {
    env_logger::init();
    let opts = Command::from_args();

    match opts.subcommand {
        SubCommand::Sli(cmd) => {
            build_sli(&cmd)?;
        }
        SubCommand::Abnormal(cmd) => {
            build_abnormal(&cmd)?;
        }
        SubCommand::Drop(cmd) => {
            build_drop(&cmd)?;
        }
    }

    Ok(())
}
