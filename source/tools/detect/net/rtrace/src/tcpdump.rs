use std::{
    fs::File,
    io::{BufReader, BufWriter, Read, Write},
    net::SocketAddrV4,
    path::PathBuf,
};

use anyhow::{bail, Result};
use std::sync::Mutex;
use structopt::StructOpt;
use utils::*;

#[derive(Debug, Clone, StructOpt)]
pub struct TcpdumpCommand {
    #[structopt(short, long, help = "File to store data")]
    output: Option<PathBuf>,
    #[structopt(short, long, help = "File to parse data")]
    input: Option<PathBuf>,
    #[structopt(long, help = "Local network address of tracing skb")]
    src: Option<String>,
    #[structopt(long, help = "Remote network address of tracing skb")]
    dst: Option<String>,
    #[structopt(long, help = "Eanble timer event")]
    timerevent: bool,
}

use sock::*;

static mut EVENTS_NUM: usize = 0;

fn parse_tcpdump_file(cmd: &TcpdumpCommand) -> bool {
    if let Some(i) = &cmd.input {
        let mut saddr = 0;
        let mut daddr = 0;
        let mut sport = 0;
        let mut dport = 0;

        if let Some(ip) = &cmd.src {
            let s: SocketAddrV4 = ip.parse().unwrap();
            saddr = u32::from_le_bytes(s.ip().octets());
            sport = s.port();
        }

        if let Some(ip) = &cmd.dst {
            let s: SocketAddrV4 = ip.parse().unwrap();
            daddr = u32::from_le_bytes(s.ip().octets());
            dport = s.port();
        }

        let mut f = File::open(i).expect("faile to open file");
        let meta = std::fs::metadata(&i).expect("unable to read metadata");
        let mut buffer = vec![0; meta.len() as usize];
        f.read(&mut buffer).expect("buffer overflow");

        // if buffer.len() % std::mem::size_of::<sock_events>() != 0 {
        //     println!("data not aligned");
        // }

        // let sock_events_sz = std::mem::size_of::<sock_events>();
        // for i in (0..buffer.len()).step_by(sock_events_sz) {
        //     let (h, b, t) = unsafe { buffer[i..i + sock_events_sz].align_to::<sock_events>() };
        //     let event = b[0];

        //     if saddr != 0 && event.ap.saddr != saddr {
        //         continue;
        //     }

        //     if daddr != 0 && event.ap.daddr != daddr {
        //         continue;
        //     }

        //     if sport != 0 && event.ap.sport != sport {
        //         continue;
        //     }

        //     if dport != 0 && event.ap.dport != dport {
        //         continue;
        //     }

        //     println!("{}", event);
        // }

        return true;
    }
    false
}

pub fn run_tcpdump(cmd: &TcpdumpCommand, debug: bool, btf: &Option<String>) {
    if parse_tcpdump_file(cmd) {
        return;
    }

    let filter = inner_sock_filter::new(&cmd.src, &cmd.dst).unwrap();
    let mut sock = Sock::new(debug, btf, filter).unwrap();
    let mut writer = None;
    if let Some(x) = &cmd.output {
        writer = Some(BufWriter::new(File::create(x).unwrap()));
    }

    let (tx, rx) = crossbeam_channel::unbounded();

    ctrlc::set_handler(move || tx.send(()).expect("Could not send signal on channel."))
        .expect("Error setting Ctrl-C handler");

    println!("Waiting for Ctrl-C...");

    if writer.is_some() {
        std::thread::spawn(|| loop {
            std::thread::sleep(std::time::Duration::from_secs(1));
            unsafe {
                println!(
                    "{} events,{} KB",
                    EVENTS_NUM,
                    EVENTS_NUM / 1024 * std::mem::size_of::<sock_event>()
                );
            }
        });
    }

    loop {
        if let Some(mut event) = sock.poll(std::time::Duration::from_millis(200)).unwrap() {
            if let Some(x) = &mut writer {
                unsafe { EVENTS_NUM += 1 };
                let slice = unsafe {
                    std::slice::from_raw_parts_mut(
                        &mut event as *mut sock_event as *mut u8,
                        std::mem::size_of::<sock_event>(),
                    )
                };
                x.write(slice).expect("failed to write event to file");
            } else {
                println!("{}", event);
            }
        }

        if !rx.is_empty() {
            println!("Exiting...");
            break;
        }
    }
}
