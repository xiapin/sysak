mod event;
mod skb;
mod sock;
mod tcp;

pub use {
    self::event::{string_to_addr_pair, AddressInfo, TcpEvent, TcpEventType, TcpUsrEvent},
    self::skb::Skb,
    self::sock::TcpSock,
    self::tcp::Tcp,
};

use anyhow::Result;
use std::collections::HashMap;
use std::rc::Rc;
use crate::latencylegacy::LatencyCommand;

fn partition_events(events: Vec<Rc<TcpUsrEvent>>) {
    let mut seqs = Vec::new();

    for event in &events {
        log::debug!("{}", event);
        let (sseq, eseq) = event.seq();
        seqs.push(sseq);
        seqs.push(eseq);
    }

    seqs.sort_unstable();
    seqs.dedup();

    for i in 1..seqs.len() {
        let mut skb_events = Vec::new();

        for event in &events {
            let (sseq, eseq) = event.seq();
            if seqs[i - 1] >= sseq && seqs[i] <= eseq {
                skb_events.push(event.clone());
            }
        }

        if skb_events.is_empty() {
            continue;
        }
        let skb = Skb::new(skb_events, (seqs[i - 1], seqs[i]));
        println!("{}", skb);
    }
}

#[derive(Default)]
struct Net {
    sock_map: HashMap<u64, (TcpSock, Option<AddressInfo>)>,
    delta: u64,
}

impl Net {
    pub fn new() -> Net {
        Net {
            sock_map: HashMap::default(),
            delta: eutils_rs::timestamp::delta_of_mono_real_time(),
        }
    }

    pub fn insert(&mut self, tcp: &mut Tcp, event: TcpEvent) -> Result<()> {
        let addr = event.sockaddr();
        let mut item = self
            .sock_map
            .entry(addr)
            .or_insert((TcpSock::new(self.delta), None));
        if item.1.is_none() {
            item.1 = tcp.listend_socks_lookup(addr)?;
        }
        item.0.handle_event(event);

        Ok(())
    }

    pub fn consume(&mut self, max_ts: u64) {
        for (_, sock) in &mut self.sock_map {
            let events = sock.0.group_send_event(max_ts);
            if !events.is_empty() {
                match &sock.1 {
                    Some(addr) => {
                        println!("{}", addr);
                        partition_events(events);
                    }
                    None => {
                        log::debug!("No AddressInfo");
                    }
                }
            }

            let events = sock.0.group_recv_event(max_ts);
            if !events.is_empty() {
                match &sock.1 {
                    Some(addr) => {
                        println!("{}", addr);
                        partition_events(events);
                    }
                    None => {
                        log::debug!("No AddressInfo");
                    }
                }
            }
        }
    }
}

pub fn build_tcp(opts: &LatencyCommand) -> Result<()> {
    let mut tcp = Tcp::new(log::log_enabled!(log::Level::Debug))?;
    let mut src = String::from("0.0.0.0:0");
    let mut dst = String::from("0.0.0.0:0");
    let mut net = Net::new();
    let delay = std::cmp::max(opts.latency * 2 * 1_000_000, 1_000_000_000);

    if let Some(x) = &opts.src {
        src = x.clone();
    }
    if let Some(x) = &opts.dst {
        dst = x.clone();
    }
    let ap = string_to_addr_pair(&src, &dst)?;

    if let Some(pid) = opts.pid {
        tcp.set_filter_pid(pid as u32);
    }
    tcp.set_filter_ap(ap);
    tcp.update_filter()?;
    tcp.attach()?;

    let mut pre_ts = 0;
    loop {
        let mut cur_ts = 0;
        if let Some(event) = tcp.poll(std::time::Duration::from_millis(100))? {
            log::debug!("{}", event);
            cur_ts = event.ts();
            net.insert(&mut tcp, event)?;
        }

        if cur_ts == 0 {
            cur_ts = eutils_rs::timestamp::current_monotime();
        }

        if cur_ts > pre_ts && cur_ts - pre_ts > delay {
            net.consume(pre_ts);
            pre_ts = cur_ts;
        }
    }
}
