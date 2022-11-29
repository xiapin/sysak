pub enum MessageType {
    MessageIcmpEvent(icmp::IcmpEvents),
}

use std::collections::BTreeMap;

#[derive(Default)]
pub struct MessageOrderedQueue {
    queue: BTreeMap<u64, Vec<MessageType>>,
}

impl MessageOrderedQueue {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn push(&mut self, ts: u64, msg: MessageType) {
        self.queue.entry(ts).or_insert(vec![]).push(msg);
    }

    pub fn pop(&mut self) -> Option<MessageType> {
        let mut ret = None;
        let mut ts = None;

        if let Some(val) = self.queue.iter_mut().next() {
            ret = val.1.pop();
            if val.1.is_empty() {
                ts = Some(*val.0);
            }
        }

        if let Some(x) = ts {
            self.queue.remove(&x);
        }
        ret
    }

    pub fn poll_enqueue(&mut self, rx: &mut crossbeam_channel::Receiver<MessageType>) {
        loop {
            match rx.recv() {
                Ok(msg) => {
                    let mut ts = 0;
                    match &msg {
                        MessageType::MessageIcmpEvent(icmp) => {
                            ts = icmp.start_ts();
                        }
                        _ => {}
                    }

                    self.push(ts, msg);
                }
                Err(_) => {}
            }
        }
    }
}
