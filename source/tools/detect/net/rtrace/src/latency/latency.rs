use crate::message::MessageType;
use anyhow::{bail, Result};
use crossbeam_channel::{Receiver, Sender};
use eutils_rs::timestamp::current_monotime;
use icmp::{IcmpEventType, IcmpEvents};
use serde::{Deserialize, Serialize};
use structopt::StructOpt;
use utils::*;

#[derive(Debug, Clone, StructOpt)]
pub struct LatencyCommand {
    #[structopt(long, default_value = "tcp", help = "Network protocol type")]
    proto: String,
    #[structopt(long, help = "enable json output format")]
    json: bool,
    #[structopt(long, default_value = "60", help = "program running time in seconds")]
    duration: usize,
}

struct Latency {
    cmd: LatencyCommand,
    debug: bool,
    btf: Option<String>,
    timeout: std::time::Duration,
    rx: Receiver<MessageType>,
    tx: Sender<MessageType>,
}

impl Latency {
    pub fn new(cmd: &LatencyCommand, debug: bool, btf: &Option<String>) -> Self {
        let (tx, rx) = crossbeam_channel::unbounded();
        Latency {
            cmd: cmd.clone(),
            debug: debug,
            btf: btf.clone(),
            timeout: std::time::Duration::from_millis(200),
            rx,
            tx,
        }
    }

    fn start_icmp_thread(&self) {
        let mut icmp = icmp::Icmp::new(self.debug, &self.btf);
        let tx = self.tx.clone();
        let timeout = self.timeout;
        std::thread::spawn(move || loop {
            if let Some(event) = icmp.poll(timeout).unwrap() {
                tx.send(MessageType::MessageIcmpEvent(event.0)).unwrap();
                tx.send(MessageType::MessageIcmpEvent(event.1)).unwrap();
            }
        });
    }

}

fn run_latency_icmp(cmd: &LatencyCommand, debug: bool, btf: &Option<String>) {
    let mut latency = Latency::new(cmd, debug, btf);
    latency.start_icmp_thread();

    let mut boot_ts = 0;
    let mut start_ts = 0;
    let mut end_ts = 0;
    let mut show_message = "".to_owned();

    let start = current_monotime();
    let duration = (cmd.duration as u64) * 1_000_000_000;

    loop {
        match latency.rx.recv_timeout(std::time::Duration::from_millis(200)) {
            Ok(event) => match event {
                MessageType::MessageIcmpEvent(icmpe) => {
                    if icmpe.events.len() == 0 {
                        continue;
                    }

                    if cmd.json {
                        println!("{}", serde_json::to_string(&icmpe).unwrap());
                    } else {
                        show_message = icmpe.to_string();
                        start_ts = icmpe.start_ts();
                        end_ts = icmpe.end_ts();
                    }
                }
                _ => {}
            },
            Err(_) => {
                if !cmd.json {
                    continue;
                }
            }
        }
        if boot_ts == 0 {
            boot_ts = start_ts;
        }

        if current_monotime() - start > duration {
            break;
        }
        if !cmd.json {
            println!(
                "SinceBootTimeDuration: {}ms -> {}ms\n{}",
                (start_ts - boot_ts) / 1_000_000,
                (end_ts - boot_ts) / 1_000_000,
                show_message
            );
        }
    }

}

pub fn run_latency(cmd: &LatencyCommand, debug: bool, btf: &Option<String>) {
    let mut latency = Latency::new(cmd, debug, btf);

    match cmd.proto.as_str() {
        "icmp" => {
            run_latency_icmp(cmd, debug, btf);
        }
        _ => {}
    }
}
