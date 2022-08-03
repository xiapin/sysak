use crate::abnormal_bindings::{addr_pair, event, net_params};

use eutils_rs::net::TcpState;

use std::net::{IpAddr, Ipv4Addr, SocketAddr, SocketAddrV4};

#[derive(Debug)]
pub struct Event {
    src: SocketAddr,
    dst: SocketAddr,
    state: TcpState,
    inum: u32,

    accept_queue: u32,
    percent_accept_queue: f64,
    syn_queue: u32,
    percent_syn_queue: f64,
    max_queue: u32,

    snd_mem: u32,
    max_snd_mem: u32,
    percent_snd_mem: f64,
    rcv_mem: u32,
    max_rcv_mem: u32,
    percent_rcv_mem: f64,

    score: f64,
}

impl Default for Event {
    fn default() -> Self {
        Event {
            src: SocketAddr::new(IpAddr::V4(Ipv4Addr::new(0, 0, 0, 0)), 0),
            dst: SocketAddr::new(IpAddr::V4(Ipv4Addr::new(0, 0, 0, 0)), 0),
            state: TcpState::Unknown,
            inum: 0,
            accept_queue: 0,
            percent_accept_queue: 0.0,
            syn_queue: 0,
            percent_syn_queue: 0.0,
            max_queue: 0,
            snd_mem: 0,
            max_snd_mem: 0,
            percent_snd_mem: 0.0,
            rcv_mem: 0,
            max_rcv_mem: 0,
            percent_rcv_mem: 0.0,
            score: 0.0,
        }
    }
}

impl Event {
    pub fn new(data: Vec<u8>) -> Event {
        let mut event = Event::default();
        let ptr = &data[0] as *const u8 as *const event;

        let mut sk_ack_backlog = unsafe { (*ptr).__bindgen_anon_1.tp.sk_ack_backlog } as f64;
        let mut sk_max_ack_backlog =
            unsafe { (*ptr).__bindgen_anon_1.tp.sk_max_ack_backlog } as f64;
        let mut icsk_accept_queue = unsafe { (*ptr).__bindgen_anon_1.tp.icsk_accept_queue } as f64;
        let mut sk_wmem_queued = unsafe { (*ptr).__bindgen_anon_1.tp.sk_wmem_queued } as f64;
        let mut sndbuf = unsafe { (*ptr).__bindgen_anon_1.tp.sndbuf } as f64;
        let mut rmem_alloc = unsafe { (*ptr).__bindgen_anon_1.tp.rmem_alloc } as f64;
        let mut sk_rcvbuf = unsafe { (*ptr).__bindgen_anon_1.tp.sk_rcvbuf } as f64;

        event.state = TcpState::from(unsafe { (*ptr).__bindgen_anon_1.tp.state } as i32);
        event.inum = unsafe { (*ptr).i_ino };

        event.accept_queue = sk_ack_backlog as u32;
        event.syn_queue = icsk_accept_queue as u32;
        event.snd_mem = sk_wmem_queued as u32;
        event.rcv_mem = rmem_alloc as u32;
        event.max_queue = sk_max_ack_backlog as u32;
        event.max_snd_mem = sndbuf as u32;
        event.max_rcv_mem = sk_rcvbuf as u32;

        match event.state {
            TcpState::Listen => {
                event.percent_accept_queue = sk_ack_backlog / sk_max_ack_backlog;
                event.percent_syn_queue = icsk_accept_queue / sk_max_ack_backlog;
            }
            _ => {
                event.percent_snd_mem = sk_wmem_queued / sndbuf;
                event.percent_rcv_mem = rmem_alloc / sk_rcvbuf;
            }
        }

        let base: f64 = 2.0;
        event.score += base.powf(event.percent_accept_queue) - 1.0;
        event.score += base.powf(event.percent_syn_queue) - 1.0;
        event.score += base.powf(event.percent_snd_mem) - 1.0;
        event.score += base.powf(event.percent_rcv_mem) - 1.0;

        let daddr = unsafe { (*ptr).ap.daddr };
        let dport = unsafe { (*ptr).ap.dport };
        let saddr = unsafe { (*ptr).ap.saddr };
        let sport = unsafe { (*ptr).ap.sport };
        event.src = SocketAddr::new(IpAddr::V4(Ipv4Addr::from(u32::from_be(saddr))), sport);
        event.dst = SocketAddr::new(IpAddr::V4(Ipv4Addr::from(u32::from_be(daddr))), dport);

        event
    }

    pub fn inum(&self) -> u32 {
        self.inum
    }

    pub fn score(&self) -> f64 {
        self.score
    }

    pub fn state(&self) -> &TcpState {
        &self.state
    }

    pub fn src(&self) -> SocketAddr {
        self.src
    }

    pub fn dst(&self) -> SocketAddr {
        self.dst
    }

    pub fn percent_accept_queue(&self) -> f64 {
        self.percent_accept_queue * 100.0
    }

    pub fn percent_syn_queue(&self) -> f64 {
        self.percent_syn_queue * 100.0
    }

    pub fn percent_snd_mem(&self) -> f64 {
        self.percent_snd_mem * 100.0
    }

    pub fn percent_rcv_mem(&self) -> f64 {
        self.percent_rcv_mem * 100.0
    }
}
