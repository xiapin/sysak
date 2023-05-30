use anyhow::{bail, Result};
use drop::{Drop, DropEvent};
use serde::{Deserialize, Serialize};
use structopt::StructOpt;
use utils::{timestamp::{current_monotime, current_realtime}, delta_netstat::{DeltaNetstat, Netstat}, delta_snmp::{DeltaSnmp, Snmp}, kernel_stack::has_kernel_symbol, delta_dev::NetDev};

#[derive(Debug, StructOpt)]
pub struct DropCommand {
    #[structopt(long, default_value = "600", help = "program running time in seconds")]
    duration: usize,
}

fn get_enabled_points(opts: &DropCommand) -> Result<Vec<(&str, bool)>> {
    let mut enabled = vec![];
    if !has_kernel_symbol("tcp_drop") {
        enabled.push(("tcp_drop", false));
    }
    Ok(enabled)
}

fn show_counter() {
    let netstat = Netstat::from_file("/proc/net/netstat").unwrap();
    let snmp = Snmp::from_file("/proc/net/snmp").unwrap();
    let dev = NetDev::new().unwrap();

    println!("{}", serde_json::to_string(&netstat).unwrap());
    println!("{}", serde_json::to_string(&snmp).unwrap());
    println!("{}", serde_json::to_string(&dev).unwrap());
}

pub fn run_drop(cmd: &DropCommand, debug: bool, btf: &Option<String>) {
    let mut drop = Drop::builder()
        .open(debug, btf)
        .load_enabled(get_enabled_points(cmd).expect("failed to get enabled points"))
        .open_perf()
        .build();

    drop.skel.attach().expect("failed to attach bpf program");
    
    let duration = (cmd.duration * 1_000_000_000) as u64;
    let start_ns = current_monotime();

    show_counter();

    loop {
        if let Some(event) = drop
            .poll(std::time::Duration::from_millis(100))
            .expect("failed to poll drop event")
        {
            println!("{}", serde_json::to_string(&event).unwrap());
        }

        if current_monotime() - start_ns >= duration {
            break;
        }
    }

    show_counter();
}
