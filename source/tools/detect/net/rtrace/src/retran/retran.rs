use anyhow::{bail, Result};
use eutils_rs::proc::Netstat;
use retran::{RetranEvent, Retran};
use structopt::StructOpt;
use utils::{timestamp::{current_monotime, current_realtime}, delta_netstat::show_netstat_json};
use serde::{Deserialize, Serialize};

#[derive(Debug, StructOpt)]
pub struct RetranCommand {
    #[structopt(long, help = "Process identifier of container")]
    pid: Option<usize>,
    #[structopt(long, default_value = "600", help = "program running time in seconds")]
    duration: usize,
}


// fn get_enabled_points() -> Result<Vec<(&'static str, bool)>> {
//     let mut enabled = vec![];
//     if eutils_rs::KernelVersion::current()? >= eutils_rs::KernelVersion::try_from("4.10.0")? {
//         enabled.push(("tp_tcp_retransmit_skb", true));
//     } else {
//         enabled.push(("__tcp_retransmit_skb", true));
//     }
//     Ok(enabled)
// }

pub fn run_retran(cmd: &RetranCommand, debug: bool, btf: &Option<String>) {
    let mut retran = Retran::builder()
        .open(debug, btf)
        .load()
        .open_perf()
        .build();

    retran.skel.attach().expect("failed to attach bpf program");

    let duration = (cmd.duration * 1_000_000_000) as u64;
    let start_ns = current_monotime();

    show_netstat_json().unwrap();

    loop {
        if let Some(event) = retran
            .poll(std::time::Duration::from_millis(100))
            .expect("failed to poll drop event")
        {
            println!("{}",serde_json::to_string(&event).unwrap());
        }

        if current_monotime() - start_ns >= duration {
            break;
        }
    }

    show_netstat_json().unwrap();
}
