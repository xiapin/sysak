



mod event;
mod icmp;


pub use {
    self::event::{IcmpEvent, IcmpEventType},
    self::icmp::Icmp,
};


use anyhow::Result;
use crate::latencylegacy::LatencyCommand;
use std::collections::HashMap;


pub fn build_icmp(opts: &LatencyCommand) -> Result<()> {
    let mut hm: HashMap<(u16, u16), Vec<IcmpEvent>> = HashMap::default();
    let mut icmp = Icmp::new(log::log_enabled!(log::Level::Debug))?;
    icmp.attach()?;

    let delta = eutils_rs::timestamp::delta_of_mono_real_time();
    log::debug!("delta: {}", delta);

    loop {
        if let Some(event) = icmp.poll(std::time::Duration::from_millis(100))? {
            let key = (event.id(), event.seq());
            let pre_key = (event.id(), event.seq() - 1);
            let item = hm.entry(key).or_insert(Vec::new());
            item.push(event);

            if let Some(x) = hm.remove(&pre_key) {
                icmp_handle_events(x, delta);
            }
        }
    }
}

fn vec_string(vec: &Vec<(u64, IcmpEventType)>) -> Option<String> {
    if vec.len() > 0 {
        let mut str = format!("{}", vec[0].1);
        for i in 1..vec.len() {
            str.push_str(&format!(
                " ->{}us-> {}",
                (vec[i].0 - vec[i - 1].0) / 1000,
                vec[i].1
            ));
        }
        return Some(str);
    }
    None
}


fn icmp_handle_events(events: Vec<IcmpEvent>, delta: u64) {
    let mut snd = Vec::new();
    let mut rcv = Vec::new();
    let mut has_ping_rcv = false;

    for event in &events {
        log::debug!("{}", event);
        let ty = event.type_();

        match ty {
            IcmpEventType::PingSnd => {
                snd.push((event.ts(), ty));
            }

            IcmpEventType::PingNetDevQueue | IcmpEventType::PingNetDevXmit => {
                if event.is_echo() {
                    snd.push((event.ts(), ty));
                }

                if event.is_echo_reply() {
                    rcv.push((event.ts(), ty));
                }
            }

            IcmpEventType::PingDevRcv => {}

            IcmpEventType::PingNetifRcv => {
                if event.is_echo() {
                    rcv.push((event.ts(), ty));
                }

                if event.is_echo_reply() {
                    snd.push((event.ts(), ty));
                }
            }

            IcmpEventType::PingIcmpRcv => {
                let mut skbts = event.skb_ts();

                if skbts > delta {
                    skbts -= delta;
                }

                if event.is_echo() {
                    rcv.push((event.ts(), ty));
                    if skbts != 0 {
                        rcv.push((skbts, IcmpEventType::PingDevRcv));
                    }
                }

                if event.is_echo_reply() {
                    snd.push((event.ts(), ty));
                    if skbts != 0 {
                        snd.push((skbts, IcmpEventType::PingDevRcv));
                    }
                }
            }

            IcmpEventType::PingRcv => {
                snd.push((event.ts(), ty));
                has_ping_rcv = true;
            }

            IcmpEventType::PingKfreeSkb => {
                if has_ping_rcv && event.is_echo_reply() {
                    snd.push((event.ts(), ty));
                }
            }

            _ => {}
        }
    }

    snd.sort_by(|a, b| a.0.cmp(&b.0));
    rcv.sort_by(|a, b| a.0.cmp(&b.0));

    if let Some(x) = vec_string(&snd) {
        println!("snd: {}", x);
    }

    if let Some(x) = vec_string(&rcv) {
        println!("rcv: {}", x);
    }
}
