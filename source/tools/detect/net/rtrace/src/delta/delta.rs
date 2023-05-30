use std::{
    fs::File,
    io::{BufReader, BufWriter, Read, Write},
    net::SocketAddrV4,
    path::PathBuf,
};

use anyhow::{bail, Result};
use structopt::StructOpt;
use utils::{delta_netstat::*, delta_snmp::*, delta_dev::*, timestamp::current_monotime};
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, StructOpt)]
pub struct DeltaCommand {
    #[structopt(long, help = "File to store data")]
    output: Option<PathBuf>,
    #[structopt(long, help = "Tracing file /proc/net/netstat")]
    netstat: bool,
    #[structopt(long, help = "Tracing file /proc/net/snmp")]
    snmp: bool,
    #[structopt(long, default_value = "1", help = "Sample period")]
    period: u64,

    #[structopt(long, help = "json format")]
    json: bool,
}

use crate::message::MessageType;


pub fn delta_snmp_thread(tx: crossbeam_channel::Sender<MessageType>, period: u64) {
    std::thread::sleep(std::time::Duration::from_millis(period));

}

#[derive(Serialize, Deserialize, Default)]
pub struct DropReasons {
    reasons: Vec<(String, isize, String)>,
}

impl DropReasons {

    pub fn add_netstat(&mut self, reason: &NetstatDropStatus) {
        self.reasons.push( 
            (reason.key.clone(), reason.count, reason.reason.clone())
        );
    }

    pub fn add_snmp(&mut self, reason: &SnmpDropStatus) {
        self.reasons.push( 
            (reason.key.clone(), reason.count, reason.reason.clone())
        );
    }

    pub fn add_dev(&mut self, reason: &DeviceDropStatus) {
        self.reasons.push(
            (format!("{}-{}", reason.dev, reason.key), reason.count, reason.reason.clone())
        );
    }
}

fn for_sysom(cmd: &DeltaCommand) {
    let mut netstat = DeltaNetstat::new("/proc/net/netstat").unwrap();
    let mut snmp = DeltaSnmp::new("/proc/net/snmp").unwrap();
    let mut dev = DeltaDev::new().unwrap();
    std::thread::sleep(std::time::Duration::from_secs(cmd.period));

    netstat.update().unwrap();
    snmp.update().unwrap();
    dev.update().unwrap();

    let mut reasons = DropReasons::default();

    for reason in netstat.drop_reason() {
        reasons.add_netstat(&reason);
    }

    for reason in snmp.drop_reason() {
        reasons.add_snmp(&reason);
    }

    for reason in dev.drop_reason() {
        reasons.add_dev(&reason);
    }

    println!("{}", serde_json::to_string(&reasons).unwrap());

}

pub fn run_delta(cmd: &DeltaCommand) {

    let mut netstat = None;
    let mut snmp = None;

    if cmd.json {
        for_sysom(cmd);
        return;
    }
    
    if cmd.netstat {
        netstat = Some(DeltaNetstat::new("/proc/net/netstat").unwrap());
    }

    if cmd.snmp {
        snmp = Some(DeltaSnmp::new("/proc/net/snmp").unwrap());
    }

    loop {
        std::thread::sleep(std::time::Duration::from_secs(cmd.period));
        if let Some(x) = &mut netstat {
            x.update().unwrap();
            println!("{} Netstat: {}", eutils_rs::timestamp::current_monotime(), x);
        }
        if let Some(x) = &mut snmp {
            x.update().unwrap();
            println!("{} Snmp: {}",eutils_rs::timestamp::current_monotime(), x);
        }
    }
}
