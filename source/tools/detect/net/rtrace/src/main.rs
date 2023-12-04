use anyhow::Result;
use rtrace::application::drop::DropApplication;
use rtrace::application::jitter::JitterApplication;
use rtrace::application::ping::PingApplication;
use rtrace::application::retran::RetranApplication;
use rtrace::application::tcpping::TcppingApplication;
use rtrace::common::config::Config;
use rtrace::common::file_logger::setup_file_logger;
use rtrace::common::protocol::Protocol;
use rtrace::common::utils::parse_ip_str;
use rtrace::event::initial_stop_event_thread;
use structopt::StructOpt;

fn parse_protocol(src: &str) -> Result<Protocol, &'static str> {
    Protocol::try_from(src)
}

fn parse_threshold(threshold: &str) -> u64 {
    let base = if threshold.ends_with("ms") {
        1_000_000
    } else if threshold.ends_with("us") {
        1000
    } else {
        panic!("Please end with ms or us")
    };

    let mut tmp = threshold.to_owned();
    tmp.pop();
    tmp.pop();

    let ns: u64 = tmp.parse().expect("not a number");

    ns * base
}

#[derive(Debug, StructOpt)]
#[structopt(name = "rtrace", about = "Diagnosing tools of kernel network")]
pub struct Command {
    #[structopt(long, help = "program running time in seconds")]
    duration: Option<u64>,
    #[structopt(short, long, default_value = "0us", parse(from_str = parse_threshold), help = "jitter threshold, ms/us")]
    threshold: u64,
    #[structopt(long, parse(from_str = parse_ip_str), help = "Network source address")]
    src: Option<(u32, u16)>,
    #[structopt(long, parse(from_str = parse_ip_str), help = "Network destination address")]
    dst: Option<(u32, u16)>,
    #[structopt(short, long, default_value = "tcp", parse(try_from_str = parse_protocol), help = "Network protocol")]
    protocol: Protocol,
    #[structopt(short, long, default_value = "1000", help = "Collection times")]
    count: u32,
    #[structopt(
        short,
        long,
        default_value = "eth0",
        help = "Monitoring virtio-net tx/rx queue"
    )]
    interface: String,
    #[structopt(long, default_value = "1", help = "Monitoring period, unit seconds")]
    period: f32,
    #[structopt(long, help = "Enable the network jitter diagnosing module")]
    jitter: bool,
    #[structopt(long, help = "Monitoring virtio-net tx/rx queue")]
    virtio: bool,
    #[structopt(long, help = "Enable the network packetdrop diagnosing module")]
    drop: bool,
    #[structopt(long, help = "Enable the network retransmission diagnosing module")]
    retran: bool,
    #[structopt(long, help = "Enable the tcpping module")]
    tcpping: bool,
    #[structopt(long, help = "Enable the ping module")]
    ping: bool,
    #[structopt(long, help = "output in json format")]
    json: bool,
    #[structopt(long, help = "Enable IQR analysis module")]
    iqr: bool,
    #[structopt(short, long, help = "Verbose debug output")]
    verbose: bool,
}

fn main() {
    let opts = Command::from_args();
    setup_file_logger(opts.verbose).expect("failed to setup file logger");

    let config = Config {
        threshold: opts.threshold,
        src: if let Some(x) = opts.src { x } else { (0, 0) },
        dst: if let Some(x) = opts.dst { x } else { (0, 0) },
        protocol: opts.protocol,
        jitter: opts.jitter,
        drop: opts.drop,
        retran: opts.retran,
        verbose: opts.verbose,
        ping: opts.ping,
        output_raw: !opts.json,
        output_json: opts.json,
        interface: opts.interface.clone(),
        period: std::time::Duration::from_millis((opts.period * 1000.0) as u64),
        virtio: opts.virtio,
        disable_kfree_skb: false,
        tcpping: opts.tcpping,
        count: opts.count,
        iqr: opts.iqr,
    };

    if let Some(d) = opts.duration {
        let dt = std::time::Duration::from_secs(d);
        initial_stop_event_thread(dt);
    }

    if config.drop {
        DropApplication::run(config);
        return;
    }

    if config.tcpping {
        TcppingApplication::run(config);
        return;
    }

    if config.ping {
        PingApplication::run(config);
        return;
    }

    if config.retran {
        RetranApplication::run(config);
        return;
    }

    if config.jitter {
        JitterApplication::run(config);
        return;
    }
}
