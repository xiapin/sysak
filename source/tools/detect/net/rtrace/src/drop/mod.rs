mod diagnosing;

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

use self::diagnosing::build_drop_reasons;
use structopt::StructOpt;

#[derive(Debug, StructOpt)]
pub struct DropCommand {
    #[structopt(long, default_value = "tcp", help = "Network protocol type")]
    proto: String,
}

pub struct DropParams {
    pub event: Option<Event>,
    pub snmp: Snmp,
    pub stack: Vec<String>,
}

impl DropParams {
    pub fn new() -> Result<DropParams> {
        Ok(DropParams {
            event: None,
            snmp: Snmp::from_file("/proc/net/snmp")?,
            stack: Vec::new(),
        })
    }

    pub fn update_snmp(&mut self) -> Result<()> {
        // self.snmp -= Snmp::from_file("/proc/net/snmp")?;
        Ok(())
    }

    pub fn set_stack(&mut self, stack: Vec<String>) {
        self.stack = stack;
    }
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
    let mut drop = Drop::new(log::log_enabled!(log::Level::Debug))?;
    let kallsyms = Kallsyms::try_from("/proc/kallsyms")?;
    let mut drop_params = DropParams::new()?;
    let mut drop_reasons = build_drop_reasons();

    drop.attach()?;

    loop {
        if let Some(event) = drop.poll(std::time::Duration::from_millis(100))? {
            log::debug!("{}", event);
            // check event is we care

            // show basic
            show_basic(&event);

            // show stack
            let stack_string = get_stack_string(&kallsyms, drop.get_stack(event.stackid())?)?;
            for ss in &stack_string {
                println!("\t{}", ss);
            }

            drop_params.set_stack(stack_string);

            drop_params.update_snmp()?;

            for dr in &drop_reasons {
                dr.init(&drop_params);
            }

            drop_reasons.sort_by(|a, b| b.probability().cmp(&a.probability()));

            //  show
            println!("诊断数据: {}", drop_reasons[0].display());
            println!("丢包原因: {}", drop_reasons[0].reason());
            println!("诊断建议: {}", drop_reasons[0].recommend());
        }
    }
}
