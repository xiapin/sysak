use crate::collector::virtio::Queue;
use crate::collector::virtio::Virtio;

#[derive(Default)]
pub struct VirtioAnalyzer {
    virtios: Vec<Virtio>,
    ftx: Vec<usize>,
    frx: Vec<usize>,
}

impl VirtioAnalyzer {
    pub fn new(virtios: Vec<Virtio>) -> Self {
        let mut v = VirtioAnalyzer::default();
        v.virtios = virtios;
        v
    }
    pub fn analysis(&mut self) {
        let mut prev = self.virtios.iter();
        let mut next = self.virtios.iter();
        next.next();

        let mut ftx = vec![];
        let mut frx = vec![];
        loop {
            if let Some(v1) = prev.next() {
                if let Some(v2) = next.next() {
                    let (tx, rx) = analysis_virtios(v1, v2);
                    if tx.is_empty() && rx.is_empty() {
                        continue;
                    }

                    ftx = tx;
                    frx = rx;

                    break;
                }
            }
            break;
        }

        self.ftx = ftx;
        self.frx = frx;
    }

    pub fn analysis_result(&self) -> String {
        let mut res = String::new();
        if !self.ftx.is_empty() {
            res += format!(
                "faulty send queue: {}",
                self.ftx
                    .iter()
                    .map(|x| x.to_string())
                    .collect::<Vec<String>>()
                    .join(",")
            )
            .as_str();
        }

        if !self.frx.is_empty() {
            res += format!(
                "faulty recv queue: {}",
                self.frx
                    .iter()
                    .map(|x| x.to_string())
                    .collect::<Vec<String>>()
                    .join(",")
            )
            .as_str();
        }
        res
    }

    pub fn print(&self) {
        let res = self.analysis_result();
        if !res.is_empty() {
            println!("{res}");
        }
    }
}

pub fn analysis_virtios(v1: &Virtio, v2: &Virtio) -> (Vec<usize>, Vec<usize>) {
    let mut rxs = vec![];
    let mut txs = vec![];
    for (idx, (q1, q2)) in v1.tx.iter().zip(v2.tx.iter()).enumerate() {
        if tpackets(q1) != 0 && tpackets(q2) != 0 && q1.last_used == q2.last_used {
            txs.push(idx);
        }
    }

    for (idx, (q1, q2)) in v1.rx.iter().zip(v2.rx.iter()).enumerate() {
        if rpackets(q1) != 0 && rpackets(q2) != 0 && q1.last_used == q2.last_used {
            rxs.push(idx);
        }
    }

    (txs, rxs)
}

fn tpackets(q: &Queue) -> u16 {
    q.avail - q.used
}

fn rpackets(q: &Queue) -> u16 {
    q.used - q.last_used
}
