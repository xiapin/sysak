use crate::abnormal::pstree::Pstree;
use anyhow::{bail, Result};
use crossbeam_channel;
use eutils_rs::net::TcpState;
use gettid::gettid;
use std::thread;
use std::time;
use structopt::StructOpt;
// use tcp::Tcp;
use crate::abnormal::skel::*;
use crate::common::*;

pub struct AbnortmalOutput {}

#[derive(Debug, StructOpt)]
pub struct AbnormalCommand {
    #[structopt(long, help = "Process identifier")]
    pid: Option<usize>,
    #[structopt(long, help = "Local network address of traced sock")]
    src: Option<String>,
    #[structopt(long, help = "Remote network address of traced sock")]
    dst: Option<String>,
    #[structopt(long, default_value = "10", help = "Show top N connections")]
    top: usize,
    #[structopt(
        long,
        default_value = "score",
        help = "Sorting key, including: synq, acceptq, rcvm, sndm, drop, retran, ooo, score"
    )]
    sort: String,
}

enum ChannelMsgType {
    Start,
    PstreeIns(Pstree),
    Pid(u32),
}

fn get_events(
    opts: &AbnormalCommand,
    debug: bool,
    btf: &Option<String>,
) -> Result<(Pstree, Vec<Event>)> {
    let (ts, tr) = crossbeam_channel::unbounded();
    let (ms, mr) = (ts.clone(), tr.clone());
    let pstree_thread = thread::spawn(move || {
        let pid = unsafe { gettid() };
        ts.send(ChannelMsgType::Pid(pid as u32)).unwrap();
        thread::sleep(time::Duration::from_millis(10));
        match tr.recv().unwrap() {
            ChannelMsgType::Start => {
                let mut pstree = Pstree::new();
                pstree.update().expect("failed to update pstree");
                ts.send(ChannelMsgType::PstreeIns(pstree))
                    .expect("failed to send pstree");
            }
            _ => {}
        }
    });

    let mut pstree_thread_pid = 0;

    match mr.recv()? {
        ChannelMsgType::Pid(pid) => {
            log::debug!(
                "main thread tid: {}, pstree thread tid: {}",
                unsafe { gettid() },
                pid
            );
            pstree_thread_pid = pid;
        }
        _ => {
            bail!("failed to get pstree thread pid")
        }
    }
    let mut events = Vec::default();

    let mut skel = Skel::default();
    skel.open_load(debug, btf, vec![], vec![])?;

    let mut filter = Filter::new();
    filter.set_ap(&opts.src, &opts.dst)?;
    filter.set_pid(pstree_thread_pid);
    filter.set_protocol(libc::IPPROTO_TCP as u16);

    skel.filter_map_update(0, unsafe {
        std::mem::transmute::<commonbinding::filter, filter>(filter.filter())
    })?;
    skel.attach()?;
    // start perf thread.
    skel.poll(std::time::Duration::from_millis(1000))?;
    ms.send(ChannelMsgType::Start)?;

    loop {
        if let Some(data) = skel.poll(std::time::Duration::from_millis(1000))? {
            events.push(Event::new(data));
        } else {
            break;
        }
    }

    let pstree;
    assert!(!mr.is_empty());
    match mr.recv()? {
        ChannelMsgType::PstreeIns(ins) => {
            log::debug!("receive pstree instance");
            pstree = ins;
        }
        _ => {
            bail!("failed to get pstree thread pid")
        }
    }

    pstree_thread
        .join()
        .expect("Couldn't join on the associated thread");
    Ok((pstree, events))
}

struct Score {
    state: TcpState,
    acceptq: u32,
    synq: u32,
    rcvm: u32,
    sndm: u32,
    drop: u32,
    retran: u32,
    ooo: u32,
    score: f64,
    idx: usize,
}

impl Default for Score {
    fn default() -> Self {
        Score {
            state: TcpState::Close,
            acceptq: 1,
            synq: 1,
            rcvm: 1,
            sndm: 1,
            drop: 1,
            retran: 1,
            ooo: 1,
            score: 0.0,
            idx: 1,
        }
    }
}

impl Score {
    pub fn new(event: &Event, idx: usize) -> Score {
        Score {
            state: TcpState::from(event.state() as i32),
            acceptq: event.accept_queue(),
            synq: event.syn_queue(),
            rcvm: event.rcv_mem(),
            sndm: event.snd_mem(),
            drop: event.drop(),
            retran: event.retran(),
            ooo: event.ooo(),
            score: 0.0,
            idx,
        }
    }

    pub fn accept_queue(&self) -> u32 {
        self.acceptq
    }
    pub fn syn_queue(&self) -> u32 {
        self.synq
    }
    pub fn rcv_mem(&self) -> u32 {
        self.rcvm
    }
    pub fn snd_mem(&self) -> u32 {
        self.sndm
    }
    pub fn drop(&self) -> u32 {
        self.drop
    }
    pub fn retran(&self) -> u32 {
        self.retran
    }
    pub fn ooo(&self) -> u32 {
        self.ooo
    }
    pub fn score(&self) -> f64 {
        self.score
    }
    pub fn idx(&self) -> usize {
        self.idx
    }

    pub fn max(&mut self, event: &Event) {
        self.acceptq = std::cmp::max(self.acceptq, event.accept_queue());
        self.synq = std::cmp::max(self.synq, event.syn_queue());
        self.rcvm = std::cmp::max(self.rcvm, event.rcv_mem());
        self.sndm = std::cmp::max(self.sndm, event.snd_mem());
        self.drop = std::cmp::max(self.drop, event.drop());
        self.retran = std::cmp::max(self.retran, event.retran());
        self.ooo = std::cmp::max(self.ooo, event.ooo());
    }

    pub fn compute_score(&mut self, max_score: &Score) {
        let base: f64 = 2.0;

        let acceptq = base.powf(self.acceptq as f64 / max_score.acceptq as f64) - 1.0;
        let synq = base.powf(self.synq as f64 / max_score.synq as f64) - 1.0;
        let rcvm = base.powf(self.rcvm as f64 / max_score.rcvm as f64) - 1.0;
        let sndm = base.powf(self.sndm as f64 / max_score.sndm as f64) - 1.0;
        let drop = base.powf(self.drop as f64 / max_score.drop as f64) - 1.0;
        let retran = base.powf(self.retran as f64 / max_score.retran as f64) - 1.0;
        let ooo = base.powf(self.ooo as f64 / max_score.ooo as f64) - 1.0;

        let mut score = 0.0;
        match self.state {
            TcpState::Listen => {
                score += acceptq * 10.0 + synq * 10.0;
            }
            _ => {
                score += (rcvm + sndm) * 5.0;
            }
        }
        score += drop * 10.0 + retran * 5.0 + ooo;

        self.score = score;
    }
}

pub fn build_abnormal(opts: &AbnormalCommand, debug: bool, btf: &Option<String>) -> Result<()> {
    let (mut pstree, mut events) = get_events(opts, debug, btf)?;
    let mut top = opts.top;

    let mut scores = Vec::new();
    let mut max_score = Score::default();
    let mut max_score_float = 0.0;

    for (idx, event) in events.iter().enumerate() {
        scores.push(Score::new(event, idx));
        max_score.max(event);
    }

    for score in &mut scores {
        score.compute_score(&max_score);
        max_score_float = f64::max(max_score_float, score.score());
    }

    match opts.sort.as_str() {
        "score" => {
            scores.sort_by(|a, b| b.score().partial_cmp(&a.score()).unwrap());
        }
        "acceptq" => {
            scores.sort_by(|a, b| b.accept_queue().cmp(&a.accept_queue()));
        }
        "synq" => {
            scores.sort_by(|a, b| b.syn_queue().cmp(&a.syn_queue()));
        }
        "rcvm" => {
            scores.sort_by(|a, b| b.rcv_mem().cmp(&a.rcv_mem()));
        }
        "sndm" => {
            scores.sort_by(|a, b| b.snd_mem().cmp(&a.snd_mem()));
        }
        "drop" => {
            scores.sort_by(|a, b| b.drop().cmp(&a.drop()));
        }
        "retran" => {
            scores.sort_by(|a, b| b.retran().cmp(&a.retran()));
        }
        "ooo" => {
            scores.sort_by(|a, b| b.ooo().cmp(&a.ooo()));
        }
        _ => {
            bail!("Sort key is unknown: {}", opts.sort)
        }
    }

    println!(
        "{:<20} {:<25} {:<25} {:<15} {:<30} {:<30} {:<30} {:<10}",
        "Pid/Comm",
        "Local",
        "Remote",
        "TCP-STATE",
        "MEMORY(sndm, rcvm)",
        "PACKET(drop, retran, ooo)",
        "Queue(synq, acceptq)",
        "Score"
    );

    for score in scores {
        if top == 0 {
            break;
        }
        top -= 1;
        // pid local remote state memory packet queue score
        let event = &events[score.idx()];
        let mut pidstring = String::from("None/None");
        match pstree.inum_pid(event.inum()) {
            Ok(pid) => match pstree.pid_comm(pid) {
                Ok(comm) => {
                    pidstring = format!("{}/{}", pid, comm);
                }
                Err(e) => {}
            },
            Err(e) => {}
        }

        let memory = format!("{}, {}", event.snd_mem(), event.rcv_mem());
        let packet = format!("{}, {}, {}", event.drop(), event.retran(), event.ooo());
        let queue = format!("{}, {}", event.syn_queue(), event.accept_queue(),);
        println!(
            "{:<20} {:<25} {:<25} {:<15} {:<30} {:<30} {:<30} {:<10}",
            pidstring,
            event.local(),
            event.remote(),
            TcpState::from(event.state() as i32),
            memory,
            packet,
            queue,
            (score.score() / max_score_float * 100.0) as u64
        )
        // println!("{:?}", event);
    }
    Ok(())
}
