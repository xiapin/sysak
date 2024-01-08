use nginx::nginx::{find_nginx_instances, Nginx};
use structopt::StructOpt;
use unity::unity_sock_send;

use crate::{ntopo::NTopo, rtrace::run_xrtrace};

mod cpu;
mod memory;
mod metrics;
mod network;
mod nginx;
mod ntopo;
mod unity;
mod utils;
mod xwatcher;
use libbpf_rs::skel::*;
mod rtrace;

#[derive(Debug, StructOpt)]
#[structopt(name = "xwatcher", about = "keep nginx healthy")]
pub struct Command {
    #[structopt(
        long,
        default_value = "30",
        help = "Set the collection period in seconds"
    )]
    duration: u32,
    #[structopt(short, long, help = "Verbose debug output")]
    verbose: bool,
}

fn main() {
    let opts = Command::from_args();
    env_logger::init();

    run_xrtrace();

    let mut nginxes = find_nginx_instances();
    assert!(nginxes.len() == 1);

    let mut builder = ntopo::NtopoSkelBuilder::default();
    builder.obj_builder.debug(opts.verbose);

    let mut open_skel = builder.open().unwrap();
    let mut skel = open_skel.load().expect("failed to load ntopo");
    skel.attach().unwrap();

    let pidsmap = libbpf_rs::MapHandle::try_clone(skel.maps().pids()).unwrap();
    let nodesmap = libbpf_rs::MapHandle::try_clone(skel.maps().nodes()).unwrap();
    let edgesmap = libbpf_rs::MapHandle::try_clone(skel.maps().edges()).unwrap();

    let mut ntopo = NTopo::new(pidsmap, nodesmap, edgesmap, &nginxes[0]);

    loop {
        std::thread::sleep(std::time::Duration::from_secs(opts.duration as u64));
        let mut metrics = vec![];
        for nginx in &mut nginxes {
            metrics.push(nginx.metrics());
        }
        metrics.push(ntopo.metrics(&nginxes[0]));
        let metrics_string = metrics.join("\n");
        unity_sock_send("/var/sysom/outline", &metrics_string);
    }
}
