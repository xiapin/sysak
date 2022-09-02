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
use crate::utils::macros::*;

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

#[derive(Debug)]
struct Score {
    state: TcpState,
    acceptq: u64,
    synq: u64,
    rcvm: u64,
    sndm: u64,
    drop: u64,
    retran: u64,
    ooo: u64,
    score: u64,
    idx: usize, // index in events vetor
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
            score: 0,
            idx: 1,
        }
    }
}

impl Score {
    pub fn new(event: &Event, idx: usize) -> Score {
        Score {
            state: TcpState::from(event.state() as i32),
            acceptq: event.accept_queue() as u64,
            synq: event.syn_queue() as u64,
            rcvm: event.rcv_mem() as u64,
            sndm: event.snd_mem() as u64,
            drop: event.drop() as u64,
            retran: event.retran() as u64,
            ooo: event.ooo() as u64,
            score: 0,
            idx,
        }
    }

    pub fn accept_queue(&self) -> u64 {
        self.acceptq
    }
    pub fn syn_queue(&self) -> u64 {
        self.synq
    }
    pub fn rcv_mem(&self) -> u64 {
        self.rcvm
    }
    pub fn snd_mem(&self) -> u64 {
        self.sndm
    }
    pub fn drop(&self) -> u64 {
        self.drop
    }
    pub fn retran(&self) -> u64 {
        self.retran
    }
    pub fn ooo(&self) -> u64 {
        self.ooo
    }
    pub fn score(&self) -> u64 {
        self.score
    }
    pub fn idx(&self) -> usize {
        self.idx
    }

    pub fn max(&mut self, score: &Score) {
        match score.state {
            TcpState::Close => {}
            _ => {
                struct_members_max_assign!(
                    self, self, score, acceptq, synq, rcvm, sndm, drop, retran, ooo
                );
            }
        }
    }

    pub fn min(&mut self, score: &Score) {
        match score.state {
            TcpState::Close => {}
            _ => {
                struct_members_min_assign!(
                    self, self, score, acceptq, synq, rcvm, sndm, drop, retran, ooo
                );
            }
        }
    }

    pub fn compute_score(&mut self, max_score: &Score, min_score: &Score) {
        let base: f64 = 2.0;

        let mut normalization = Score::default();
        let precision = 10000;
        struct_members_normalization_assign!(
            normalization,
            self,
            min_score,
            max_score,
            precision,
            score,
            acceptq,
            synq,
            rcvm,
            sndm,
            drop,
            retran,
            ooo
        );

        let mut total_weight_score = 0;
        let mut score = 0;
        match self.state {
            TcpState::Listen => {
                score += normalization.acceptq * 10 + normalization.synq * 10;
                total_weight_score = precision * 10 * 2;
            }
            _ => {
                score += (normalization.sndm + normalization.rcvm) * 5;
                total_weight_score = precision * 5 * 2;
            }
        }

        match self.state {
            TcpState::Close => {}
            _ => {
                score += normalization.drop * 10 + normalization.retran * 5 + normalization.ooo;
                total_weight_score = precision * (10 + 5 + 1);
            }
        }

        self.score = score * 100 / total_weight_score;
    }
}

pub fn build_abnormal(opts: &AbnormalCommand, debug: bool, btf: &Option<String>) -> Result<()> {
    let (mut pstree, mut events) = get_events(opts, debug, btf)?;
    let mut top = opts.top;

    let mut scores = Vec::new();
    let mut max_score = Score::default();
    let mut min_score = Score::default();

    for (idx, event) in events.iter().enumerate() {
        let score = Score::new(event, idx);
        max_score.max(&score);
        min_score.min(&score);
        scores.push(score);
    }

    for score in &mut scores {
        score.compute_score(&max_score, &min_score);
    }

    match opts.sort.as_str() {
        "score" => {
            scores.sort_by(|a, b| b.score().cmp(&a.score()));
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
            score.score()
        )
        // println!("{:?}", event);
    }
    Ok(())
}
