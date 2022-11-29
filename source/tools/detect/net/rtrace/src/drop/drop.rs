use anyhow::{bail, Result};
use drop::{Drop, DropEvent, DropFilter};
use serde::{Deserialize, Serialize};
use structopt::StructOpt;
use utils::timestamp::{current_monotime, current_realtime};

#[derive(Debug, StructOpt)]
pub struct DropCommand {
    #[structopt(long, default_value = "all", help = "Network protocol type", parse(try_from_str = utils::net::parse_protocol))]
    proto: u16,
    #[structopt(long, help = "Process identifier of container")]
    pid: Option<usize>,
    #[structopt(long, help = "Local network address of traced sock")]
    src: Option<String>,
    #[structopt(long, help = "Remote network address of traced sock")]
    dst: Option<String>,

    #[structopt(long, help = "Enable monitoring iptables modules")]
    iptables: bool,
    #[structopt(long, help = "Enable monitoring conntrack modules")]
    conntrack: bool,
    #[structopt(long, help = "Enable monitoring kfree_skb")]
    kfree: bool,
    #[structopt(long, help = "Enable monitoring tcp_drop")]
    tcpdrop: bool,

    #[structopt(
        long,
        default_value = "3",
        help = "Period of display in seconds. 0 mean display immediately when event triggers"
    )]
    period: u64,

    #[structopt(
        long,
        default_value = "1000",
        help = "The maximum number of occurrences of packet drop events"
    )]
    count: usize,
    #[structopt(long, default_value = "600", help = "program running time in seconds")]
    duration: usize,

    #[structopt(long, help = "output in json format")]
    json: bool,
}

#[derive(Serialize, Deserialize)]
pub struct DropJson {
    total_drop: usize,
    record_drop: usize,
    drops: Vec<String>,
}

impl DropJson {
    pub fn new() -> Self {
        DropJson {
            total_drop: 0,
            record_drop: 0,
            drops: Vec::new(),
        }
    }

    pub fn add_drop(&mut self, event: &DropEvent) {
        if self.total_drop > 1000 {
            return;
        }

        self.total_drop += 1;
        self.drops.push(event.to_string());
    }
}

fn get_enabled_points(opts: &DropCommand) -> Result<Vec<(&str, bool)>> {
    let mut enabled = vec![];

    if opts.conntrack {
        enabled.push(("__nf_conntrack_confirm", true));
        enabled.push(("__nf_conntrack_confirm_ret", true));
    }

    if opts.iptables {
        if eutils_rs::KernelVersion::current()? >= eutils_rs::KernelVersion::try_from("4.10.0")? {
            enabled.push(("ipt_do_table", true));
        } else {
            enabled.push(("ipt_do_table310", true));
        }
        enabled.push(("ipt_do_table_ret", true));
    }

    if opts.kfree {
        if eutils_rs::KernelVersion::current()? >= eutils_rs::KernelVersion::try_from("5.10.0")? {
            enabled.push(("kfree_skb", true));
        } else {
            enabled.push(("kfree_skb", true));
        }
    }

    if opts.tcpdrop {
        if eutils_rs::KernelVersion::current()? >= eutils_rs::KernelVersion::try_from("5.10.0")? {
            enabled.push(("kfree_skb", true));
        } else {
            enabled.push(("tcp_drop", true));
        }
    }

    if enabled.is_empty() {
        bail!("please specify tracing point, such as --tcpdrop, --kfree etc.");
    }

    Ok(enabled)
}

pub fn run_drop(cmd: &DropCommand, debug: bool, btf: &Option<String>) {
    let mut drop = Drop::builder()
        .open(debug, btf)
        .load_enabled(get_enabled_points(cmd).expect("failed to get enabled points"))
        .open_perf()
        .build();

    let mut filter = DropFilter::new();

    filter
        .set_ap(&cmd.src, &cmd.dst)
        .expect("failed to parse socket address");

    filter.set_protocol(cmd.proto);

    drop.skel
        .maps_mut()
        .filter_map()
        .update(
            &utils::to_vec::<u32>(0),
            &filter.to_vec(),
            libbpf_rs::MapFlags::ANY,
        )
        .expect("failed to update filter map");

    drop.skel.attach().expect("failed to attach bpf program");

    let mut event_count = 0;
    let duration = (cmd.duration * 1_000_000_000) as u64;
    let start_ns = current_monotime();
    let mut drop_json = DropJson::new();

    loop {
        if let Some(event) = drop
            .poll(std::time::Duration::from_millis(100))
            .expect("failed to poll drop event")
        {
            event_count += 1;
            if cmd.json {
                drop_json.add_drop(&event);
            } else {
                println!("{}", event);
            }
        }

        if current_monotime() - start_ns >= duration {
            break;
        }

        if event_count >= cmd.count {
            break;
        }
    }

    if cmd.json {
        println!("{}", serde_json::to_string(&drop_json).unwrap());
    }
}
