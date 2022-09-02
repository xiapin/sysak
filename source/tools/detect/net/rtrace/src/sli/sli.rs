use crate::perf::PerfBufferBuilder;
use crate::sli::event::{Event, LatencyHist};
use crate::sli::bindings::*;
use crate::sli::skel::*;
use anyhow::{bail, Result};
use crossbeam_channel;
use crossbeam_channel::Receiver;
use eutils_rs::proc::Kallsyms;
use libbpf_rs::{MapFlags, Link};
use once_cell::sync::Lazy;
use std::sync::Mutex;
use std::time::Duration;

use eutils_rs::proc::Snmp;
use crate::sli::skel::*;


    pub fn attach_latency(&mut self) -> Result<()> {
        let ksyms = Kallsyms::try_from("/proc/kallsyms")?;
        if ksyms.has_sym("tcp_rtt_estimator") {
            self.skel.links.kprobe__tcp_rtt_estimator =
                Some(self.skel.progs_mut().kprobe__tcp_rtt_estimator().attach()?);
        } else {
            self.skel.links.kprobe__tcp_ack =
                Some(self.skel.progs_mut().kprobe__tcp_ack().attach()?);
        }
        Ok(())
    }

    pub fn attach_applatency(&mut self) -> Result<()> {
        if eutils_rs::KernelVersion::current()? < eutils_rs::KernelVersion::try_from("3.11.0")? {
        } else {
            self.skel.links.tp__tcp_probe = Some(self.skel.progs_mut().tp__tcp_probe().attach()?);
            self.skel.links.tp__tcp_rcv_space_adjust =
                Some(self.skel.progs_mut().tp__tcp_rcv_space_adjust().attach()?);
        }
        Ok(())
    }

    pub fn attach_drop(&mut self) -> Result<()> {
        if eutils_rs::KernelVersion::current()? >= eutils_rs::KernelVersion::try_from("5.10.0")? {
            self.skel.links.kfree_skb = Some(
                self.skel
                    .progs_mut()
                    .kfree_skb()
                    .attach_kprobe(false, "kfree_skb_reason")?,
            );
        } else {
            self.skel.links.kfree_skb = Some(self.skel.progs_mut().kfree_skb().attach()?);
            self.skel.links.tcp_drop = Some(self.skel.progs_mut().tcp_drop().attach()?);
        }
        Ok(())
    }

}

use structopt::StructOpt;

#[derive(Debug, StructOpt)]
pub struct SliCommand {
    #[structopt(long, help = "Collect drop metrics")]
    drop: bool,

    #[structopt(long, help = "Collect retransmission metrics")]
    retran: bool,

    #[structopt(long, help = "Collect latency metrics")]
    latency: bool,
    #[structopt(
        short,
        long,
        help = "Collect latency between kernel and application in receiving side"
    )]
    applat: bool,
    #[structopt(
        long,
        default_value = "1000",
        help = "Max latency to trace, default is 1000ms"
    )]
    threshold: u32,

    #[structopt(long, default_value = "3", help = "Data collection cycle, in seconds")]
    period: u64,

    #[structopt(long, help = "Output every sli to shell")]
    shell: bool,

    #[structopt(long, help = "Custom btf path")]
    btf: Option<String>,
}


pub fn build_sli(opts: &SliCommand) -> Result<()> {
    let mut old_snmp = Snmp::from_file("/proc/net/snmp")?;
    let delta_ns = opts.period * 1_000_000_000;
    // let mut sli = Sli::new( opts.threshold)?;
    let mut skel = Skel::default();
    let mut sli_output: SliOutput = unsafe { std::mem::MaybeUninit::zeroed().assume_init() };
    let mut pre_ts = 0;
    let mut has_output = false;

    // sli.open_load(log::log_enabled!(log::Level::Debug), &opts.btf, vec![], vec![])?;

    let mut openskel = skel.open(log::log_enabled!(log::Level::Debug), &opts.btf)?;
    let mut enabled = vec![];
    if opts.latency {

        let ksyms = Kallsyms::try_from("/proc/kallsyms")?;
        if ksyms.has_sym("tcp_rtt_estimator") {
            enabled.push("kprobe__tcp_rtt_estimator");
        } else {
            enabled.push("kprobe__tcp_ack");
        }

        // sli.attach_latency()?;
        // sli.lookup_and_update_latency_map(0)?;
    }

    if opts.applat {
        if eutils_rs::KernelVersion::current()? < eutils_rs::KernelVersion::try_from("3.11.0")? {
        } else {
            enabled.push("tp__tcp_probe");
            enabled.push("tp__tcp_rcv_space_adjust");
        }
        // sli.attach_applatency()?;
        // sli.lookup_and_update_latency_map(1)?;
    }

    if opts.drop {
        // sli.attach_drop()?;
        enabled.push("tcp_drop");
    }

    loop {
        if let Some(event) = sli.poll(std::time::Duration::from_millis(100))? {
            // log::debug!("{}", event);
            sli_output.events.push(event);
        }

        let cur_ts = eutils_rs::timestamp::current_monotime();
        if cur_ts - pre_ts < delta_ns {
            continue;
        }

        pre_ts = cur_ts;

        let new_snmp = Snmp::from_file("/proc/net/snmp")?;

        has_output = false;

        if opts.retran {
            sli_output.outsegs = snmp_delta(&old_snmp, &new_snmp, ("Tcp:", "OutSegs"))?;
            sli_output.retran = snmp_delta(&old_snmp, &new_snmp, ("Tcp:", "RetransSegs"))?;

            if opts.shell {
                println!(
                    "OutSegs: {}, Retran: {}, %Retran: {}",
                    sli_output.outsegs,
                    sli_output.retran,
                    sli_output.retran as f64 / sli_output.outsegs as f64
                );
                has_output = true;
            }
        }

        if opts.latency {
            if let Some(x) = sli.lookup_and_update_latency_map(0)? {
                sli_output.latencyhist = x;
            }

            if opts.shell {
                println!("rtt histogram");
                println!("{}", sli_output.latencyhist);
                for event in &sli_output.events {
                    match event {
                        Event::LatencyEvent(le) => {
                            println!("{}", le);
                        }
                        _ => {}
                    }
                }
                has_output = true;
            }
        }

        if opts.applat {
            if opts.shell {
                let mut first = true;
                for event in &sli_output.events {
                    match event {
                        Event::AppLatencyEvent(le) => {
                            if first {
                                has_output = true;
                                first = false;
                                println!("application latency event:");
                            }
                            println!("{}", le);
                        }
                        _ => {}
                    }
                }
            }
        }

        if opts.drop {
            if opts.shell {
                let mut first = true;
                for event in &sli_output.events {
                    match event {
                        Event::DropEvent(de) => {
                            if first {
                                has_output = true;
                                first = false;
                                println!("packet drop event:");
                            }
                            println!("{}", de);
                        }
                        _ => {}
                    }
                }
            }
        }

        old_snmp = new_snmp;
        // clear
        sli_output = unsafe { std::mem::MaybeUninit::zeroed().assume_init() };
        if has_output {
            println!("\n\n");
        }
    }
}



