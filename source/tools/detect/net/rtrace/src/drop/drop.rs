use crate::common::*;
use crate::drop::skel::*;
use crate::stack::Stack;
use crate::utils::*;
use crate::Command;
use anyhow::Result;
use byteorder::{NativeEndian, ReadBytesExt};
use chrono::prelude::*;
use eutils_rs::proc::{Kallsyms, Netstat};
use eutils_rs::{net::ProtocolType, net::TcpState, proc::Snmp};
use std::collections::HashMap;
use std::fmt;
use structopt::StructOpt;

#[derive(Debug, StructOpt)]
pub struct DropCommand {
    #[structopt(long, default_value = "tcp", help = "Network protocol type")]
    proto: String,
    #[structopt(long, help = "Process identifier of container")]
    pid: Option<usize>,
    #[structopt(long, help = "Local network address of traced sock")]
    src: Option<String>,
    #[structopt(long, help = "Remote network address of traced sock")]
    dst: Option<String>,

    #[structopt(long, help = "Enable monitoring iptables modules")]
    iptables: bool,
    #[structopt(long, help = "Enable monitoring conntrack modules")]
    conntrack: bool,
    #[structopt(long, help = "Enable monitoring kfree_skb")]
    kfree: bool,
    #[structopt(long, help = "Enable monitoring tcp_drop")]
    tcpdrop: bool,

    #[structopt(
        long,
        default_value = "3",
        help = "Period of display in seconds. 0 mean display immediately when event triggers"
    )]
    period: u64,
}

fn get_enabled_points(opts: &DropCommand) -> Result<Vec<&str>> {
    let mut enabled = vec![];

    if opts.conntrack {
        enabled.push("__nf_conntrack_confirm");
        enabled.push("__nf_conntrack_confirm_ret");
    }

    if opts.iptables {
        if eutils_rs::KernelVersion::current()? >= eutils_rs::KernelVersion::try_from("4.10.0")? {
            enabled.push("ipt_do_table");
        } else {
            enabled.push("ipt_do_table310");
        }
        enabled.push("ipt_do_table_ret");
    }

    if opts.kfree {
        if eutils_rs::KernelVersion::current()? >= eutils_rs::KernelVersion::try_from("5.10.0")? {
            enabled.push("kfree_skb");
        } else {
            enabled.push("kfree_skb");
        }
    }

    if opts.tcpdrop {
        if eutils_rs::KernelVersion::current()? >= eutils_rs::KernelVersion::try_from("5.10.0")? {
            enabled.push("kfree_skb");
        } else {
            enabled.push("tcp_drop");
        }
    }

    Ok(enabled)
}

pub fn build_drop(opts: &DropCommand, debug: bool, btf: &Option<String>) -> Result<()> {
    let kallsyms = Kallsyms::try_from("/proc/kallsyms")?;

    // load, set filter and attach
    let mut filter = Filter::new();
    filter.set_ap(&opts.src, &opts.dst)?;

    let mut skel = Skel::default();
    let enabled = get_enabled_points(&opts)?;
    skel.open_load_enabled(debug, btf, enabled)?;
    skel.filter_map_update(0, unsafe {
        std::mem::transmute::<commonbinding::filter, filter>(filter.filter())
    })?;
    skel.attach()?;

    let mut drop = Drop::new(&opts.pid)?;
    // poll data
    let mut pre_ts = 0;
    loop {
        if let Some(data) = skel.poll(std::time::Duration::from_millis(100))? {
            let event = Event::new(data);

            let key = unsafe { std::mem::transmute::<u32, [u8; 4]>(event.stackid()) };
            let stack = Stack::new(Vec::new());
            match skel
                .skel
                .maps_mut()
                .stackmap()
                .lookup(&key, libbpf_rs::MapFlags::ANY)
            {
                Ok(stack) => {}
                Err(e) => {}
            }

            drop.insert_event(event, stack);
        }

        let cur_ts = eutils_rs::timestamp::current_monotime();
        if cur_ts - pre_ts > opts.period * 1_000_000_000 {
            pre_ts = cur_ts;
            drop.update()?;
            println!("{}", drop);
            drop.clear_events();
        }
    }
}

pub struct Drop {
    delta_snmp: DeltaSnmp,
    delta_netstat: DeltaNetstat,
    delta_dev: DeltaDev,
    events: Vec<(Event, Stack)>,
}

impl Drop {
    pub fn new(pid: &Option<usize>) -> Result<Drop> {
        let mut snmp_path = "/proc/net/snmp".to_owned();
        let mut netstat_path = "/proc/net/netstat".to_owned();
        if let Some(x) = pid {
            snmp_path = format!("/proc/{}/net/snmp", x);
            netstat_path = format!("/proc/{}/net/netstat", x);
        }
        Ok(Drop {
            delta_snmp: DeltaSnmp::new(&snmp_path)?,
            delta_netstat: DeltaNetstat::new(&netstat_path)?,
            delta_dev: DeltaDev::new()?,
            events: Vec::default(),
        })
    }

    pub fn update(&mut self) -> Result<()> {
        self.delta_snmp.update()?;
        self.delta_netstat.update()?;
        self.delta_dev.update()?;
        Ok(())
    }

    pub fn insert_event(&mut self, event: Event, stack: Stack) {
        self.events.push((event, stack));
    }

    pub fn clear_events(&mut self) {
        self.events.clear();
    }
}

impl fmt::Display for Drop {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        writeln!(f, "Total {} packets drop", self.events.len())?;

        for x in &self.events {
            let event = &x.0;
            writeln!(
                f,
                "{} SKB INFO: protocol: {} \t{} -> {}",
                Utc::now().format("%Y-%m-%d %H:%M:%S"),
                event.protocol(),
                event.local(),
                event.remote()
            )?;
            let skap = event.skap();
            writeln!(
                f,
                "\t \t SOCK INFO: protocol: {} \t {} -> {}, \t state: {}",
                event.sk_protocol(),
                skap.0,
                skap.1,
                TcpState::from(event.state() as i32),
            )?;

            match event.event_type() {
                EventType::DropIptables => {
                    println!(
                        "Table:{} Chain:{}",
                        event.iptables_name(),
                        event.iptables_chain_name()
                    );
                }
                _ => {}
            }
        }

        writeln!(f, "Delta Snmp\n{}", self.delta_snmp)?;
        writeln!(f, "Delta Netstat\n{}", self.delta_netstat)?;
        writeln!(f, "Delta Device Status\n{}", self.delta_dev)?;
        write!(f, "")
    }
}
