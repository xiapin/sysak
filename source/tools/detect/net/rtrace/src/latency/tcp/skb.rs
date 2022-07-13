use crate::latency::tcp::TcpUsrEvent;
use std::fmt;
use std::rc::Rc;

pub struct Skb {
    events: Vec<Rc<TcpUsrEvent>>,
    delay: u64,
    seq: (usize, usize),
}

impl Skb {
    pub fn new(mut events: Vec<Rc<TcpUsrEvent>>, seq: (usize, usize)) -> Skb {
        events.sort_by(|a, b| a.ts().cmp(&b.ts()));
        let delay = events.last().unwrap().ts() - events.first().unwrap().ts();

        Skb {
            events,
            delay,
            seq,
        }
    }

    pub fn delay(&self) -> u64 {
        self.delay
    }
}

impl fmt::Display for Skb {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        // write!(
        //     f,
        //     "sequence: ({}, {}), delay: {}\n",
        //     self.seq.0, self.seq.1, self.delay
        // )?;
        write!(f, "{}", self.events[0].type_())?;
        for i in 1..self.events.len() {
            write!(
                f,
                " ->{}us-> {}",
                (self.events[i].ts() - self.events[i - 1].ts()) / 1000,
                self.events[i].type_()
            )?;
        }
        Ok(())
    }
}

