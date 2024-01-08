use crate::collector::drop::DropCollector;
use crate::collector::netdev::NetDev;
use crate::collector::netstat::Netstat;
use crate::collector::ping::PingSender;
use crate::collector::queueslow::QueueSlowCollector;
use crate::collector::snmp::Snmp;
use crate::collector::tcpping::TcppingCollector;
use crate::collector::userslow::UserSlowCollector;
use crate::collector::virtio::VirtioCollector;
use crate::common::config::Config;
use crate::common::utils::detect_rps;
use crate::common::utils::get_host_ipv4;
use crate::common::utils::is_virtio_net;
use crate::event::Event;
use anyhow::Result;
use crossbeam_channel::Sender;
use std::net::Ipv4Addr;

use super::retran::RetranCollector;

pub fn initial_collector_thread_drop(config: &Config, tx: Sender<Event>) {
    log::debug!("inital drop collector thread");
    let mut dp = DropCollector::new(
        config.verbose,
        config.protocol,
        config.src.0,
        config.dst.0,
        config.src.1,
        config.dst.1,
    );

    std::thread::spawn(move || {
        dp.poll(tx);
    });
}

pub fn initial_collector_thread_tcpping(config: &Config, tx: Sender<Event>) {
    let (dsti, dport) = config.dst;
    let src = if config.src.0 == 0 {
        get_host_ipv4()
    } else {
        Ipv4Addr::from(u32::from_be(config.src.0))
    };
    let dst = Ipv4Addr::from(u32::from_be(dsti));
    let interval = config.period;
    let count = config.count;
    let sport = config.src.1;
    let verbose = config.verbose;

    log::debug!("inital tcpping collector thread");
    std::thread::spawn(move || {
        let mut tp = TcppingCollector::new(verbose);
        tp.ping(tx, interval, count, sport, dport, src, dst);
    });
}

pub fn initial_collector_thread_virtio(config: &Config, tx: Sender<Event>) {
    if !is_virtio_net(&config.interface) {
        panic!("unsupport non-virtio net: {}", config.interface);
    }

    let interface = config.interface.clone();
    let period = config.period;
    let verbose = config.verbose;

    log::debug!("inital virtio collector thread");
    std::thread::spawn(move || {
        let mut vp = VirtioCollector::new(verbose, interface);
        let _ = vp.refresh();
        loop {
            std::thread::sleep(period);
            let v = vp.refresh();
            tx.send(Event::Virtio(v)).unwrap();
        }
    });
}

pub fn initial_collector_thread_ping(config: &Config, tx: Sender<Event>) {
    if detect_rps() {
        log::warn!("It is strongly recommended to turn off rps before using this function");
    }

    log::debug!("inital ping collector thread");
    let mut sender = PingSender::new(config.verbose);
    std::thread::spawn(move || {
        sender.poll(tx);
    });
}

pub fn initial_collector_thread_retran(config: &Config, tx: Sender<Event>) {
    let mut rt = RetranCollector::new(config.verbose);
    std::thread::spawn(move || {
        rt.poll(tx);
    });
}

pub fn initial_collector_thread_userslow(config: &Config, tx: Sender<Event>) {
    log::debug!("inital userslow collector thread");
    let mut us = UserSlowCollector::new(config.verbose, config.threshold);
    std::thread::spawn(move || {
        us.poll(tx);
    });
}

pub fn initial_collector_thread_queueslow(config: &Config, tx: Sender<Event>) {
    log::debug!("inital queueslow collector thread");
    let mut qs = QueueSlowCollector::new(config.verbose, config.protocol, config.threshold);
    std::thread::spawn(move || {
        qs.poll(tx);
    });
}

pub fn initial_collector_netstat() -> Result<Netstat> {
    Netstat::new()
}

pub fn initial_collector_snmp() -> Result<Snmp> {
    Snmp::new()
}

pub fn initial_collector_netdev() -> Result<NetDev> {
    NetDev::new()
}
