use crate::bindings::*;
use crate::skel::*;
use crate::IcmpEvent;
use crate::IcmpEventType;
use crate::IcmpEvents;
use anyhow::{bail, Result};
use builder::SkelBuilder;
use once_cell::sync::Lazy;
use std::collections::HashMap;
use std::sync::Mutex;
use std::time::Duration;

#[derive(SkelBuilder)]
pub struct Icmp<'a> {
    pub skel: IcmpSkel<'a>,
    rx: Option<crossbeam_channel::Receiver<(usize, Vec<u8>)>>,
    events: HashMap<(u16, u16), Vec<icmp_event>>,
    delta: u64,
}

impl<'a> Icmp<'a> {
    // open, load and attach
    pub fn new(debug: bool, btf: &Option<String>) -> Icmp<'a> {
        let mut icmp = Icmp::builder()
            .open(debug, btf)
            .load()
            .open_perf()
            .attach()
            .build();
        icmp.delta = utils::timestamp::delta_of_mono_real_time();
        icmp
    }

    fn process_event(&mut self, ie: icmp_event) -> Option<(IcmpEvents, IcmpEvents)> {
        let key = (ie.id, ie.seq);
        self.events.entry(key).or_insert(vec![]).push(ie);

        if key.1 == 0 {
            return None;
        }

        let prevkey = (key.0, key.1 - 1);
        if let Some(events) = self.events.remove(&prevkey) {
            let mut send = IcmpEvents::new(true, prevkey.0, prevkey.1);
            let mut recv = IcmpEvents::new(false, prevkey.0, prevkey.1);

            for event in &events {
                let is_echo = event.icmp_type == 8; // ICMP_ECHO:8  ICMP_ECHOREPLY:0
                let ty = IcmpEventType::try_from(event.type_ as u32).expect("wrong icmp type");
                let ts = event.ts;
                let cpu = event.cpu;
                let pid = event.pid;

                let mut idx = 0;
                for i in &event.comm {
                    if *i == '\0' as u8 {
                        break;
                    }
                    idx += 1;
                }
                let comm = unsafe { String::from_utf8_unchecked(event.comm[..idx].to_vec()) };

                let mut ie = IcmpEvent::new(cpu, ty, ts);
                ie.set_pid(pid);
                ie.set_comm(comm);

                match ty {
                    IcmpEventType::PingSnd => {
                        send.push(ie);
                    }

                    IcmpEventType::PingNetDevQueue | IcmpEventType::PingNetDevXmit => {
                        if is_echo {
                            send.push(ie);
                        } else {
                            recv.push(ie);
                        }
                    }

                    IcmpEventType::PingDevRcv => {}

                    IcmpEventType::PingNetifRcv => {
                        if is_echo {
                            recv.push(ie);
                        } else {
                            send.push(ie);
                        }
                    }

                    IcmpEventType::PingIcmpRcv => {
                        let mut skbts = event.skb_ts;

                        if skbts > self.delta {
                            skbts -= self.delta;
                        }

                        if is_echo {
                            recv.push(ie);
                            // if skbts != 0 {
                            //     recv.push(IcmpEvent::new(cpu, IcmpEventType::PingDevRcv, skbts));
                            // }
                        } else {
                            send.push(ie);
                            // if skbts != 0 {
                            //     send.push(IcmpEvent::new(cpu, IcmpEventType::PingDevRcv, skbts));
                            // }
                        }
                    }

                    IcmpEventType::PingRcv => {
                        send.push(ie);
                    }
                }
            }

            send.sort();
            recv.sort();
            return Some((send, recv));
        }

        None
    }

    // return two events: sender and receiver
    pub fn poll(&mut self, timeout: Duration) -> Result<Option<(IcmpEvents, IcmpEvents)>> {
        if let Some(rx) = &self.rx {
            match rx.recv_timeout(timeout) {
                Ok(mut data) => {
                    // https://stackoverflow.com/questions/42499049/transmuting-u8-buffer-to-struct-in-rust
                    let (head, body, tail) = unsafe { data.1.align_to_mut::<icmp_event>() };
                    assert!(head.is_empty(), "Data was not aligned");

                    let mut ie = body[0];
                    ie.cpu = data.0 as u16;
                    return Ok(self.process_event(ie));
                }
                Err(_) => return Ok(None),
            }
        }
        bail!("perf channel receiver is none")
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_latency_icmp_basic() {
        utils::bump_memlock_rlimit().unwrap();
        let mut icmp = Icmp::new(true, &None);
        loop {
            if let Some(event) = icmp.poll(std::time::Duration::from_millis(2000)).unwrap() {
                println!("{} {}", event.0, event.1);
                return;
            }
        }
    }

    #[test]
    fn test_latency_icmp_icmp_event_type() {

        let ty = IcmpEventType::PingDevRcv;
    }
}

