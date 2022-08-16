#[path = "latencylegacy/bpf/.output/icmp.skel.rs"]
pub mod icmpskel;
#[path = "latencylegacy/bpf/.output/tcp.skel.rs"]
pub mod tcpskel;

#[path = "latencylegacy/bpf/.output/bindings.rs"]
#[allow(non_upper_case_globals)]
#[allow(non_camel_case_types)]
#[allow(non_snake_case)]
#[allow(dead_code)]
mod bindings;

#[path = "drop/bpf/.output/drop.skel.rs"]
pub mod dropskel;

#[path = "drop/bpf/.output/bindings.rs"]
#[allow(non_upper_case_globals)]
#[allow(non_camel_case_types)]
#[allow(non_snake_case)]
#[allow(dead_code)]
mod drop_bindings;

#[path = "abnormal/bpf/.output/bindings.rs"]
#[allow(non_upper_case_globals)]
#[allow(non_camel_case_types)]
#[allow(non_snake_case)]
#[allow(dead_code)]
mod abnormal_bindings;
#[path = "abnormal/bpf/.output/tcp.skel.rs"]
pub mod abnormaltcpskel;

#[path = "sli/bpf/.output/bindings.rs"]
#[allow(non_upper_case_globals)]
#[allow(non_camel_case_types)]
#[allow(non_snake_case)]
#[allow(dead_code)]
mod sli_bindings;
#[path = "sli/bpf/.output/sli.skel.rs"]
pub mod sliskel;

mod abnormal;
mod drop;
mod latencylegacy;
mod perf;
mod sli;

use anyhow::{bail, Result};
use drop::build_drop;
use abnormal::build_abnormal;
use latencylegacy::build_latency;
use sli::build_sli;
use structopt::StructOpt;

use latencylegacy::LatencyCommand;
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
    #[structopt(name = "latency_legacy", about = "Packet latency diagnosing")]
    LatencyLegacy(LatencyCommand),
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
        SubCommand::LatencyLegacy(cmd) => {
            build_latency(&cmd)?;
        }
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
