mod drop;
mod event;
pub use {self::drop::Drop, self::event::Event};

use crate::Command;
use anyhow::Result;
use byteorder::{NativeEndian, ReadBytesExt};
use chrono::prelude::*;
use eutils_rs::proc::Kallsyms;
use eutils_rs::{net::ProtocolType, net::TcpState, proc::Snmp};
use std::io::Cursor;

use structopt::StructOpt;

#[derive(Debug, StructOpt)]
pub struct DropCommand {
    #[structopt(long, default_value = "tcp", help = "Network protocol type")]
    proto: String,
    #[structopt(long, help = "Custom btf path")]
    btf: Option<String>,

    #[structopt(long, help = "Enable iptables modules")]
    iptables: bool,
    #[structopt(long, help = "Enable conntrack modules")]
    conntrack: bool,
    #[structopt(long, help = "Network hardware drop packet")]
    netdev: bool,
    #[structopt(long, help = "Trace common drop point")]
    drop: bool,

    #[structopt(
        long,
        default_value = "3",
        help = "Period of display in seconds. 0 mean display immediately when event triggers"
    )]
    period: u64,
}

fn show_basic(event: &Event) {
    let ap = event.ap();
    println!(
        "{} SKB INFO: protocol: {} \t{} -> {}",
        Utc::now().format("%Y-%m-%d %H:%M:%S"),
        ProtocolType::from(event.protocol() as i32),
        ap.0,
        ap.1,
    );

    let skap = event.skap();
    println!(
        "\t \t SOCK INFO: protocol: {} \t {} -> {}, \t state: {}",
        ProtocolType::from(event.sk_protocol() as i32),
        skap.0,
        skap.1,
        TcpState::from(event.state() as i32),
    )
}

fn get_stack_string(kallsyms: &Kallsyms, stack: Vec<u8>) -> Result<Vec<String>> {
    let stack_depth = stack.len() / 8;
    let mut rdr = Cursor::new(stack);
    let mut stackstring = Vec::new();
    for i in 0..stack_depth {
        let addr = rdr.read_u64::<NativeEndian>()?;
        if addr == 0 {
            break;
        }
        stackstring.push(kallsyms.addr_to_sym(addr));
    }
    Ok(stackstring)
}

pub fn build_drop(opts: &DropCommand) -> Result<()> {
    let mut drop = Drop::new(log::log_enabled!(log::Level::Debug), &opts.btf)?;
    let kallsyms = Kallsyms::try_from("/proc/kallsyms")?;
    let mut events = Vec::new();
    let mut pre_snmp = eutils_rs::proc::Snmp::from_file("/proc/net/snmp")?;
    // let mut pre_netstat = eutils_rs::proc::Netstat:from_file("/proc/net/netstat")?;
    // let mut pre_netstat;

    drop.attach()?;

    let mut pre_ts = 0;
    loop {
        if let Some(event) = drop.poll(std::time::Duration::from_millis(100))? {
            log::debug!("{}", event);
            events.push(event);
        }

        let cur_ts = eutils_rs::timestamp::current_monotime();
        if cur_ts - pre_ts > opts.period * 1_000_000_000 {
            pre_ts = cur_ts;

            println!("Total {} packets drop", events.len());
            for event in &events {
                // show basic
                show_basic(event);
                // show stack
                let stack_string = get_stack_string(&kallsyms, drop.get_stack(event.stackid())?)?;
                for ss in &stack_string {
                    println!("\t{}", ss);
                }
            }

            if opts.period == 0 && events.len() == 0 {
                continue;
            }

            let cur_snmp = pre_snmp;
            pre_snmp = Snmp::from_file("/proc/net/snmp")?;
            let delta = pre_snmp.clone() - cur_snmp;
            println!("Snmp Information");
            delta.show_non_zero();

            // Netstat::from_file("/proc/net/netstat")?;
            println!("Netstat Information");
            // show_snmp();
            // show_netstat();
        }
    }
}
