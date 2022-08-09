mod drop;
mod event;
pub use {self::drop::Drop, self::event::Event};

use crate::drop::event::EventType;
use crate::Command;
use anyhow::Result;
use byteorder::{NativeEndian, ReadBytesExt};
use chrono::prelude::*;
use eutils_rs::proc::{Kallsyms, Netstat};
use eutils_rs::{net::ProtocolType, net::TcpState, proc::Snmp};
use procfs::net::DeviceStatus;
use std::collections::HashMap;
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

    #[structopt(
        long,
        default_value = "3",
        help = "Period of display in seconds. 0 mean display immediately when event triggers"
    )]
    period: u64,

    #[structopt(long, help = "disable kfree_skb")]
    dkfree: bool,
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

pub fn show_dev_delta(dev1: &HashMap<String, DeviceStatus>, dev2: &HashMap<String, DeviceStatus>) {
    let mut stat1: Vec<_> = dev1.values().collect();

    stat1.sort_by_key(|s| &s.name);

    println!(
        "{:<10} {:<10} {:<10} {:<10} {:<10} {:<10}",
        "Interface", "SendErrs", "SendDrop", "SendFifo", "SendColls", "SendCarrier"
    );

    for stat in &stat1 {
        println!(
            "{:<10} {:<10} {:<10} {:<10} {:<10} {:<10}",
            stat.name,
            dev2.get(&stat.name).unwrap().sent_errs - stat.sent_errs,
            dev2.get(&stat.name).unwrap().sent_drop - stat.sent_drop,
            dev2.get(&stat.name).unwrap().sent_fifo - stat.sent_fifo,
            dev2.get(&stat.name).unwrap().sent_colls - stat.sent_colls,
            dev2.get(&stat.name).unwrap().sent_carrier - stat.sent_carrier,
        );
    }

    println!(
        "{:<10} {:<10} {:<10} {:<10} {:<10}",
        "Interface", "RecvErrs", "RecvDrop", "RecvFifo", "RecvFrameErr"
    );

    for stat in &stat1 {
        println!(
            "{:<10} {:<10} {:<10} {:<10} {:<10}",
            stat.name,
            dev2.get(&stat.name).unwrap().recv_errs - stat.recv_errs,
            dev2.get(&stat.name).unwrap().recv_drop - stat.recv_drop,
            dev2.get(&stat.name).unwrap().recv_fifo - stat.recv_fifo,
            dev2.get(&stat.name).unwrap().recv_frame - stat.recv_frame,
        );
    }
}

pub fn build_drop(opts: &DropCommand) -> Result<()> {
    let mut drop = Drop::new(log::log_enabled!(log::Level::Debug), &opts.btf)?;
    let kallsyms = Kallsyms::try_from("/proc/kallsyms")?;
    let mut events = Vec::new();
    let mut pre_snmp = eutils_rs::proc::Snmp::from_file("/proc/net/snmp")?;
    let mut pre_netstat = eutils_rs::proc::Netstat::from_file("/proc/net/netstat")?;
    // let mut pre_netstat;
    let mut pre_dev = procfs::net::dev_status()?;

    if !opts.dkfree {
        drop.attach_kfreeskb()?;
    }
    drop.attach_tcpdrop()?;

    if opts.iptables {
        drop.attach_iptables()?;
    }

    if opts.conntrack {
        drop.attach_conntrack()?;
    }

    let mut pre_ts = 0;
    loop {
        if let Some(event) = drop.poll(std::time::Duration::from_millis(100))? {
            log::debug!("{}", event);
            let mut stack_string = Vec::new();
            match drop.get_stack(event.stackid()) {
                Ok(stack) => {
                    stack_string = get_stack_string(&kallsyms, stack)?;
                }
                Err(e) => stack_string.push(format!("{}", e)),
            }
            events.push((event, stack_string));
        }

        let cur_ts = eutils_rs::timestamp::current_monotime();
        if cur_ts - pre_ts > opts.period * 1_000_000_000 {
            pre_ts = cur_ts;

            println!("Total {} packets drop", events.len());
            for (event, stack_string) in &events {
                // show basic
                show_basic(event);
                // show stack
                for ss in stack_string {
                    println!("\t{}", ss);
                }

                match event.type_() {
                    EventType::Iptables => {
                        println!("{}", event.iptables_params());
                    }
                    _ => {}
                }
            }

            if opts.period == 0 && events.len() == 0 {
                continue;
            }
            events.clear();

            println!("DELTA_SNMP");
            let cur_snmp = pre_snmp;
            pre_snmp = Snmp::from_file("/proc/net/snmp")?;
            let delta = pre_snmp.clone() - cur_snmp;
            delta.show_non_zero();

            // Netstat::from_file("/proc/net/netstat")?;
            println!("DELTA_NETSTAT");
            let cur_netstat = pre_netstat;
            pre_netstat = Netstat::from_file("/proc/net/netstat")?;
            let delta_netstat = pre_netstat.clone() - cur_netstat;
            delta_netstat.show_non_zero();

            // netdev
            println!("DELTA_DEV");
            let cur_dev = procfs::net::dev_status()?;
            show_dev_delta(&pre_dev, &cur_dev);
            pre_dev = cur_dev;

            println!("");
        }
    }
}
