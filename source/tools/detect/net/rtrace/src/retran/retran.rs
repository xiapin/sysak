use anyhow::{bail, Result};
use retran::{RetranEvent, Retran};
use structopt::StructOpt;
use utils::timestamp::{current_monotime, current_realtime};
use serde::{Deserialize, Serialize};

#[derive(Debug, StructOpt)]
pub struct RetranCommand {
    #[structopt(long, help = "Process identifier of container")]
    pid: Option<usize>,
    #[structopt(long, help = "Local network address of traced sock")]
    src: Option<String>,
    #[structopt(long, help = "Remote network address of traced sock")]
    dst: Option<String>,
    #[structopt(long, default_value = "600", help = "program running time in seconds")]
    duration: usize,
}


fn get_enabled_points() -> Result<Vec<(&'static str, bool)>> {
    let mut enabled = vec![];
    if eutils_rs::KernelVersion::current()? >= eutils_rs::KernelVersion::try_from("4.10.0")? {
        enabled.push(("tp_tcp_retransmit_skb", true));
    } else {
        enabled.push(("tcp_retransmit_skb", true));
    }
    Ok(enabled)
}

pub fn run_retran(cmd: &RetranCommand, debug: bool, btf: &Option<String>) {
    let mut retran = Retran::builder()
        .open(debug, btf)
        .load_enabled(get_enabled_points().expect("failed to get enabled points"))
        .open_perf()
        .build();

    // let mut filter = DropFilter::new();

    // filter
    //     .set_ap(&cmd.src, &cmd.dst)
    //     .expect("failed to parse socket address");

    // retran.skel
    //     .maps_mut()
    //     .filter_map()
    //     .update(
    //         &utils::to_vec::<u32>(0),
    //         &filter.to_vec(),
    //         libbpf_rs::MapFlags::ANY,
    //     )
    //     .expect("failed to update filter map");

    retran.skel.attach().expect("failed to attach bpf program");

    let mut event_count = 0;
    let duration = (cmd.duration * 1_000_000_000) as u64;
    let start_ns = current_monotime();

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

    
}
