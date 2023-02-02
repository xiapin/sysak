mod latency;
use crate::latency::latency::{run_latency, LatencyCommand};

mod delta;
use crate::delta::delta::{run_delta, DeltaCommand};

mod drop;
use crate::drop::drop::{run_drop, DropCommand};

mod retran;
use crate::retran::{run_retran, RetranCommand};

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
    #[structopt(name = "drop", about = "Packet drop diagnosing")]
    Drop(DropCommand),
    #[structopt(name = "latency", about = "Packet latency tracing")]
    Latency(LatencyCommand),
    #[structopt(name = "delta", about = "Store or display delta information")]
    Delta(DeltaCommand),
    #[structopt(name = "retran", about = "Tracing retransmit")]
    Retran(RetranCommand),
}

fn main() -> Result<()> {
    env_logger::init();
    let opts = Command::from_args();

    let mut btf = opts.btf.clone();
    if btf.is_none() {
        if let Ok(sysak) = std::env::var("SYSAK_WORK_PATH") {
            if let Ok(info) = uname::uname() {
                btf = Some(format!("{}/tools/vmlinux-{}", sysak, info.release));
                log::debug!("{:?}", btf);
            }
        }
    }
    eutils_rs::helpers::bump_memlock_rlimit()?;

    match opts.subcommand {
        SubCommand::Drop(cmd) => {
            run_drop(&cmd, opts.verbose, &btf);
        }
        SubCommand::Latency(cmd) => {
            run_latency(&cmd, opts.verbose, &btf);
        }
        SubCommand::Delta(cmd) => {
            run_delta(&cmd);
        }
        SubCommand::Retran(cmd) => {
            run_retran(&cmd, opts.verbose, &btf);
        }
    }

    Ok(())
}
