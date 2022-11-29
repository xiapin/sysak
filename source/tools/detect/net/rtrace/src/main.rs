mod latency;
use crate::latency::latency::{run_latency, LatencyCommand};

mod delta;
use crate::delta::delta::{run_delta, DeltaCommand};

mod drop;
use crate::drop::drop::{run_drop, DropCommand};

mod retran;
use crate::retran::{RetranCommand, run_retran};

use anyhow::{bail, Result};
use structopt::StructOpt;


mod message;
use std::path::Path;

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
<<<<<<< HEAD
    #[structopt(name = "abnormal", about = "Abnormal connection diagnosing")]
    Abnormal(AbnormalCommand),
=======
>>>>>>> 5d143e18600de5c4873b9ba1dc4e02fc282dae96
    #[structopt(name = "drop", about = "Packet drop diagnosing")]
    Drop(DropCommand),
    #[structopt(name = "latency", about = "Packet latency tracing")]
    Latency(LatencyCommand),
<<<<<<< HEAD
    // #[structopt(name = "latencylegacy", about = "Packet latency tracing(legacy version)")]
    // LatencyLegacy(LatencyLegacyCommand),
    // #[structopt(name = "sli", about = "Collection machine sli")]
    // Sli(SliCommand),
    #[structopt(name = "retran", about = "Packet retransmission tracing")]
=======
    #[structopt(name = "delta", about = "Store or display delta information")]
    Delta(DeltaCommand),
    #[structopt(name = "retran", about = "Tracing retransmit")]
>>>>>>> 5d143e18600de5c4873b9ba1dc4e02fc282dae96
    Retran(RetranCommand),
}

fn main() -> Result<()> {
    env_logger::init();
    let opts = Command::from_args();

    let mut btf = opts.btf.clone();
    eutils_rs::helpers::bump_memlock_rlimit()?;

    match opts.subcommand {
<<<<<<< HEAD
        SubCommand::Abnormal(cmd) => {
            build_abnormal(&cmd, opts.verbose, &opts.btf)?;
        }
=======
>>>>>>> 5d143e18600de5c4873b9ba1dc4e02fc282dae96
        SubCommand::Drop(cmd) => {
            run_drop(&cmd, opts.verbose, &btf);
        }
        SubCommand::Latency(cmd) => {
            run_latency(&cmd, opts.verbose, &btf);
        }
<<<<<<< HEAD
        // SubCommand::LatencyLegacy(cmd) => {
        //     build_latency_legacy(&cmd, opts.verbose, &opts.btf)?;
        // }
        // SubCommand::Sli(cmd) => {
        //     build_sli(&cmd)?;
        // }
=======
        SubCommand::Delta(cmd) => {
            run_delta(&cmd);
        }
>>>>>>> 5d143e18600de5c4873b9ba1dc4e02fc282dae96
        SubCommand::Retran(cmd) => {
            run_retran(&cmd, opts.verbose, &btf);
        }
    }

    Ok(())
}
