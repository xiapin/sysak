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
    #[structopt(long, default_value = "3", help = "program running time in seconds")]
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


#[derive(Serialize, Deserialize, Default)]
struct LatencySenderJson {
    send: u64,
    out: u64,
    recv: u64,
}

#[derive(Serialize, Deserialize, Default)]
struct LatencyReceiverJson {
    recv: u64,
    send: u64,
}

#[derive(Serialize, Deserialize, Default)]
struct LatencyIcmpJson {
    sender: Vec<LatencySenderJson>,
    receiver: Vec<LatencyReceiverJson>,

    send_event_count: usize,
    recv_envet_count: usize,
    partial_event_count: usize,

    sender_max: LatencySenderJson,
    sender_min: LatencySenderJson,
    sender_avg: LatencySenderJson,

    receiver_max: LatencyReceiverJson,
    receiver_min: LatencyReceiverJson,
    receiver_avg: LatencyReceiverJson,
}

fn sender_json(events: &IcmpEvents) -> Option<LatencySenderJson> {
    let mut ping_send = None;
    let mut dev_xmit = None;
    let mut netif_rcv = None;
    let mut ping_rcv = None;
    for (idx, event) in events.events.iter().enumerate() {
        match event.ty {
            IcmpEventType::PingSnd => {
                if ping_send.is_none() {
                    ping_send = Some(idx);
                }
            }
            IcmpEventType::PingNetDevXmit => {
                dev_xmit = Some(idx);
            }
            IcmpEventType::PingNetifRcv => {
                netif_rcv = Some(idx);
            }
            IcmpEventType::PingRcv => {
                ping_rcv = Some(idx);
            }
            _ => {}
        }
    }

    if ping_send.is_none() || dev_xmit.is_none() || netif_rcv.is_none() || ping_rcv.is_none() {
        return None;
    }

    Some(LatencySenderJson {
        send: events.events[dev_xmit.unwrap()].ts - events.events[ping_send.unwrap()].ts,
        out: events.events[netif_rcv.unwrap()].ts - events.events[dev_xmit.unwrap()].ts,
        recv: events.events[ping_rcv.unwrap()].ts - events.events[netif_rcv.unwrap()].ts,
    })
}

fn receiver_json(events: &IcmpEvents) -> Option<LatencyReceiverJson> {
    let mut dev_xmit = None;
    let mut netif_rcv = None;
    let mut icmp_rcv = None;
    for (idx, event) in events.events.iter().enumerate() {
        match event.ty {
            IcmpEventType::PingNetDevXmit => {
                dev_xmit = Some(idx);
            }
            IcmpEventType::PingNetifRcv => {
                if netif_rcv.is_none() {
                    netif_rcv = Some(idx);
                }
            }
            IcmpEventType::PingIcmpRcv => {
                icmp_rcv = Some(idx);
            }
            _ => {}
        }
    }

    if dev_xmit.is_none() || netif_rcv.is_none() || icmp_rcv.is_none() {
        return None;
    }

    Some(LatencyReceiverJson {
        send: events.events[dev_xmit.unwrap()].ts - events.events[icmp_rcv.unwrap()].ts,
        recv: events.events[icmp_rcv.unwrap()].ts - events.events[netif_rcv.unwrap()].ts,
    })
}

impl LatencyIcmpJson {
    pub fn add_event(&mut self, events: &IcmpEvents) {
        if events.sender {
            if let Some(json) = sender_json(events) {
                self.sender.push(json);
            } else {
                self.partial_event_count += 1;
            }
        } else {
            if let Some(json) = receiver_json(events) {
                self.receiver.push(json);
            } else {
                self.partial_event_count += 1;
            }
        }
    }

    pub fn stat(&mut self) {

        self.recv_envet_count = self.receiver.len();
        self.send_event_count = self.sender.len();
        let mut t1 = 0;
        let mut t2 = 0;
        let mut t3 = 0;
        if !self.sender.is_empty() {
            self.sender_min.send  = u64::MAX;
            self.sender_min.out  = u64::MAX;
            self.sender_min.recv  = u64::MAX;
            for send in &self.sender {
                self.sender_max.send = std::cmp::max(self.sender_max.send, send.send);
                self.sender_max.out = std::cmp::max(self.sender_max.out, send.out);
                self.sender_max.recv = std::cmp::max(self.sender_max.recv, send.recv);
    
                self.sender_min.send = std::cmp::min(self.sender_min.send, send.send);
                self.sender_min.out = std::cmp::min(self.sender_min.out, send.out);
                self.sender_min.recv = std::cmp::min(self.sender_min.recv, send.recv);
    
                t1 += send.send;
                t2 += send.out;
                t3 += send.recv;
            }
    
            self.sender_avg.send = t1 / self.sender.len() as u64;
            self.sender_avg.out = t2 / self.sender.len() as u64;
            self.sender_avg.recv = t3 / self.sender.len() as u64;
        }

        if !self.receiver.is_empty() {
            t1 = 0;
            t3 = 0;
            self.receiver_min.send = u64::MAX;
            self.receiver_min.recv = u64::MAX;
            for recv in &self.receiver {
                self.receiver_max.send = std::cmp::max(self.receiver_max.send, recv.send);
                self.receiver_max.recv = std::cmp::max(self.receiver_max.recv, recv.recv);
    
                self.receiver_min.send = std::cmp::min(self.receiver_min.send, recv.send);
                self.receiver_min.recv = std::cmp::min(self.receiver_min.recv, recv.recv);
    
                t1 += recv.send;
                t3 += recv.recv;
            }
    
            self.receiver_avg.send = t1 / self.receiver.len() as u64;
            self.receiver_avg.recv = t3 / self.receiver.len() as u64;
        }
        
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
    let mut latency_json = LatencyIcmpJson::default();

    loop {
        match latency.rx.recv_timeout(std::time::Duration::from_millis(200)) {
            Ok(event) => match event {
                MessageType::MessageIcmpEvent(icmpe) => {
                    if icmpe.events.len() == 0 {
                        continue;
                    }

                    if cmd.json {
                        latency_json.add_event(&icmpe);
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

    latency_json.stat();
    println!("{}", serde_json::to_string(&latency_json).unwrap());
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
