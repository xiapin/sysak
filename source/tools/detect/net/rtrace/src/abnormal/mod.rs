mod event;
mod pstree;
mod tcp;

use anyhow::{bail, Result};
use crossbeam_channel;
use event::Event;
use pstree::Pstree;
use std::thread;
use structopt::StructOpt;
use tcp::Tcp;
use std::time;
use gettid::gettid;

pub struct AbnortmalOutput {}

#[derive(Debug, StructOpt)]
pub struct AbnormalCommand {
    #[structopt(
        long,
        default_value = "tcp",
        help = "Network protocol type, now only support tcp"
    )]
    proto: String,
    #[structopt(long, help = "Process identifier")]
    pid: Option<usize>,
    #[structopt(long, help = "Local network address of traced sock")]
    src: Option<String>,
    #[structopt(long, help = "Remote network address of traced sock")]
    dst: Option<String>,
    #[structopt(long, default_value = "10", help = "Show top N connections")]
    top: usize,
}

enum ChannelMsgType {
    Start,
    PstreeIns(Pstree),
    Pid(u32),
}

fn get_events(opts: &AbnormalCommand) -> Result<Vec<Event>> {
    let (ts, tr) = crossbeam_channel::unbounded();
    let (ms, mr) = (ts.clone(), tr.clone());
    thread::spawn(move || {
        let pid = unsafe { gettid() };
        ts.send(ChannelMsgType::Pid(pid as u32)).unwrap();
        thread::sleep(time::Duration::from_millis(10));
        match tr.recv().unwrap() {
            ChannelMsgType::Start => {
                let mut pstree = Pstree::new();
                pstree.update().unwrap();
                ts.send(ChannelMsgType::PstreeIns(pstree)).unwrap();
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

    match opts.proto.as_str() {
        "tcp" => {
            let mut tcp = Tcp::new(log::log_enabled!(log::Level::Debug))?;
            tcp.set_filter_pid(pstree_thread_pid);
            tcp.set_filter_protocol(libc::IPPROTO_TCP as u16);
            tcp.set_filter_ts(eutils_rs::timestamp::current_monotime());
            tcp.update_filter()?;
            tcp.attach()?;
            // start perf thread.
            tcp.poll(std::time::Duration::from_millis(1000))?;
            ms.send(ChannelMsgType::Start)?;

            loop {
                if let Some(event) = tcp.poll(std::time::Duration::from_millis(1000))? {
                    log::debug!("{:?}", event);
                    events.push(event);
                } else {
                    break;
                }
            }
        }
        _ => {
            bail!("not support, only support tcp now")
        }
    }
    Ok(events)
}

pub fn build_abnormal(opts: &AbnormalCommand) -> Result<()> {
    let mut events = get_events(opts)?;
    let mut top = opts.top;
    events.sort_by(|a, b| b.score().partial_cmp(&a.score()).unwrap());

    println!(
        "{:<25} {:<25} {:<15} {:<7} {:<7} {:<7} {:<7} {:<7}",
        "Local", "Remote", "TCP-STATE", "%ACCQ", "%SYNQ", "%SNDMEM", "%RCVMEM", "%SCORE",
    );

    for event in events {
        if top == 0 {
            break;
        }
        top -= 1;
        // local remote state accq synq sndmem rcvmem score
        println!(
            "{:<25} {:<25} {:<15} {:<7.2} {:<7.2} {:<7.2} {:<7.2} {:<7.2}",
            event.src(),
            event.dst(),
            event.state(),
            event.percent_accept_queue(),
            event.percent_syn_queue(),
            event.percent_snd_mem(),
            event.percent_rcv_mem(),
            event.score()
        )
        // println!("{:?}", event);
    }
    Ok(())
}
