use once_cell::sync::Lazy;
use rtrace::collector::drop::disable_tp_kfree_skb;
use rtrace::collector::drop::Drop;
use rtrace::collector::launcher::initial_collector_thread_drop;
use rtrace::collector::launcher::initial_collector_thread_retran;
use rtrace::collector::retran::Retran;
use rtrace::common::config::Config;
use rtrace::common::utils::current_monotime;
use rtrace::event::get_event_channel;
use rtrace::event::Event;
use std::cmp::Ordering;
use std::collections::{BTreeMap, HashMap};
use std::net::Ipv4Addr;
use std::net::SocketAddr;
use std::ops::Bound::Included;
use std::sync::Mutex;

static GLOBAL_XRTRACE: Lazy<Mutex<XRtrace>> = Lazy::new(|| {
    let rtrace = XRtrace::new();
    Mutex::new(rtrace)
});

fn get_config() -> Config {
    let mut config = Config::default();
    config.set_protocol_tcp();
    config.enable_drop();
    config.enable_retran();
    config.disable_drop_kfree_skb();
    config
}

pub struct XRtrace {
    drops: BTreeMap<u64, Drop>,
    retrans: BTreeMap<u64, Retran>,
}

impl XRtrace {
    pub fn new() -> Self {
        disable_tp_kfree_skb();
        let config = get_config();
        let (tx, rx) = get_event_channel();
        initial_collector_thread_drop(&config, tx.clone());
        initial_collector_thread_retran(&config, tx);
        std::thread::spawn(move || loop {
            match rx.recv() {
                Ok(Event::Drop(drop)) => {
                    let ts = current_monotime();
                    GLOBAL_XRTRACE.lock().unwrap().drops.insert(ts, drop);
                }
                Ok(Event::Retran(r)) => {
                    let ts = current_monotime();
                    GLOBAL_XRTRACE.lock().unwrap().retrans.insert(ts, r);
                }
                _ => {
                    panic!("unexpected event")
                }
            }
        });

        XRtrace {
            drops: Default::default(),
            retrans: Default::default(),
        }
    }

    fn clear(&mut self) {
        self.drops.clear();
        self.retrans.clear();
    }

    fn collect(
        &mut self,
        raddr: Ipv4Addr,
        uaddr: Ipv4Addr,
        uport: u16,
        times: (u64, u64),
    ) -> String {
        let mut drop_count = 0;
        let mut retran_count = HashMap::new();
        let l = Included(times.0);
        let r = Included(times.1);
        for (_, d) in self.drops.range((l, r)) {
            match d.src {
                SocketAddr::V4(i) => {
                    if i.ip().cmp(&raddr) == Ordering::Equal {
                        drop_count += 1;
                    }
                }
                _ => panic!("ipv6 not support"),
            }

            match d.dst {
                SocketAddr::V4(i) => {
                    if i.ip().cmp(&uaddr) == Ordering::Equal && i.port() == uport {
                        drop_count += 1;
                    }
                }
                _ => panic!("ipv6 not support"),
            }
        }

        for (_, r) in self.retrans.range((l, r)) {
            match r.src {
                SocketAddr::V4(i) => {
                    if i.ip().cmp(&raddr) == Ordering::Equal {
                        retran_count
                            .entry(r.retran_type.clone())
                            .and_modify(|x| *x += 1)
                            .or_insert(1);
                    }
                }
                _ => panic!("ipv6 not support"),
            }

            match r.dst {
                SocketAddr::V4(i) => {
                    if i.ip().cmp(&uaddr) == Ordering::Equal && i.port() == uport {
                        retran_count
                            .entry(r.retran_type.clone())
                            .and_modify(|x| *x += 1)
                            .or_insert(1);
                    }
                }
                _ => panic!("ipv6 not support"),
            }
        }

        let del = times.0 - 1_000_000_000;
        self.drops = self.drops.split_off(&del);
        self.retrans = self.retrans.split_off(&del);

        let mut lines = vec![];
        if drop_count != 0 {
            lines.push(format!("该请求丢包数为:{}", drop_count));
        }

        for (k, v) in retran_count {
            lines.push(format!("{}重传{}次", k, v));
        }

        lines.join(",")
    }
}

pub fn clear_xrtrace() {
    GLOBAL_XRTRACE.lock().unwrap().clear();
}

pub fn run_xrtrace() {
    clear_xrtrace()
}

pub fn xtrace_collect(raddr: Ipv4Addr, uaddr: Ipv4Addr, uport: u16, times: (u64, u64)) -> String {
    GLOBAL_XRTRACE
        .lock()
        .unwrap()
        .collect(raddr, uaddr, uport, times)
}
