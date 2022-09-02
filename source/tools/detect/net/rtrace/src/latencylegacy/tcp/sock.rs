use crate::latencylegacy::tcp::{TcpEvent, TcpEventType, TcpUsrEvent};
use std::collections::BTreeMap;
use std::rc::Rc;

#[derive(Default)]
struct BMapValue {
    events: Vec<Rc<TcpUsrEvent>>,
    max_ts: u64,
    max_end_seq: usize,
}

impl BMapValue {
    pub fn push_event(&mut self, seq: (usize, usize), event: Rc<TcpUsrEvent>) {
        self.max_ts = std::cmp::max(self.max_ts, event.ts());
        self.max_end_seq = std::cmp::max(self.max_end_seq, seq.1);
        self.events.push(event);
    }

    pub fn expire(&self, expire: u64) -> bool {
        self.max_ts <= expire
    }
}

fn bm_insert(bm: &mut BTreeMap<usize, BMapValue>, seq: (usize, usize), event: Rc<TcpUsrEvent>) {
    let mut value = bm.entry(seq.0).or_insert(BMapValue::default());
    (*value).push_event(seq, event);
}

fn bm_group(bm: &mut BTreeMap<usize, BMapValue>, max_ts: u64) -> Vec<Rc<TcpUsrEvent>> {
    let mut events = Vec::new();
    let mut keys = Vec::new();
    let mut max_end_seq = 0;

    // println!("bm_group: bmap len is {}, max ts is {}", bm.len(), max_ts);
    for (key, value) in bm.iter() {
        if value.expire(max_ts) {
            return events;
        }

        if max_end_seq == 0 {
            max_end_seq = value.max_end_seq;
            keys.push(*key);
            continue;
        }

        if *key >= max_end_seq {
            // All func data from min_start_seq to max_end_seq have been found.
            // And the key is not included.
            break;
        } else {
            max_end_seq = std::cmp::max(max_end_seq, value.max_end_seq);
        }
        keys.push(*key);
    }

    for key in keys {
        let val = bm.remove(&key);
        if let Some(v) = val {
            events.extend(v.events);
        }
    }
    events
}

#[derive(Default)]
pub struct TcpSock {
    send: BTreeMap<usize, BMapValue>,
    recv: BTreeMap<usize, BMapValue>,

    delta: u64,
}

impl TcpSock {
    pub fn new(delta: u64) -> TcpSock {
        TcpSock {
            send: BTreeMap::default(),
            recv: BTreeMap::default(),
            delta,
        }
    }

    pub fn handle_event(&mut self, mut event: TcpEvent) {
        let seq = (event.seq() as usize, event.end_seq() as usize);
        let ack = (event.snd_una() as usize, event.ack_seq() as usize);

        let ty = event.type_();
        let ts = event.ts();

        if seq.1 == seq.0 {
            log::debug!("skip event which is ack package");
            return;
        }

        match ty {
            TcpEventType::TcpSendMsg => {
                let usr_event = TcpUsrEvent::new(ts, seq, ty);
                bm_insert(&mut self.send, seq, Rc::new(usr_event));
            }
            TcpEventType::TcpTransmitSkb
            | TcpEventType::TcpNetDevXmit
            | TcpEventType::TcpIpQueueXmit => {
                let usr_event = TcpUsrEvent::new(ts, seq, ty);
                bm_insert(&mut self.send, seq, Rc::new(usr_event));
            }
            TcpEventType::TcpAck => {
                let mut skbts = event.skbts();
                if skbts > self.delta {
                    skbts -= self.delta;
                }
                let usr_event1 = TcpUsrEvent::new(skbts, ack, TcpEventType::TcpDevRcv);
                let usr_event2 = TcpUsrEvent::new(ts, ack, ty);
                bm_insert(&mut self.send, ack, Rc::new(usr_event1));
                bm_insert(&mut self.send, ack, Rc::new(usr_event2));
            }
            TcpEventType::TcpQueueRcv => {
                let mut skbts = event.skbts();
                if skbts > self.delta {
                    skbts -= self.delta;
                }
                let usr_event1 = TcpUsrEvent::new(skbts, seq, TcpEventType::TcpDevRcv);
                let usr_event2 = TcpUsrEvent::new(ts, seq, ty);
                bm_insert(&mut self.recv, seq, Rc::new(usr_event1));
                bm_insert(&mut self.recv, seq, Rc::new(usr_event2));
            }
            TcpEventType::TcpCleanupRbuf => {
                let usr_event = TcpUsrEvent::new(ts, seq, ty);
                bm_insert(&mut self.recv, seq, Rc::new(usr_event));
            }
            _ => {}
        }
    }

    pub fn group_send_event(&mut self, max_ts: u64) -> Vec<Rc<TcpUsrEvent>> {
        let events = bm_group(&mut self.send, max_ts);
        events
    }

    pub fn group_recv_event(&mut self, max_ts: u64) -> Vec<Rc<TcpUsrEvent>> {
        bm_group(&mut self.recv, max_ts)
    }
}
