include!(concat!(env!("OUT_DIR"), "/ntopo.skel.rs"));
include!(concat!(env!("OUT_DIR"), "/ntopo.rs"));

use anyhow::{bail, Result};
use libbpf_rs::skel::*;
use structopt::StructOpt;
mod edge;
mod node;
mod pid;
use local_ip_address::local_ip;
mod sock;

use crate::{
    edge::get_edges,
    node::get_nodes,
    pid::Pids,
    sock::{unix_sock_recv, unix_sock_send},
};

#[derive(Debug, StructOpt)]
#[structopt(name = "ntopo", about = "network topology and response time")]
pub struct Command {
    #[structopt(long, help = "tracing pid list")]
    pids: Vec<u32>,
    #[structopt(long, help = "Enable tracing mysql")]
    enable_mysql: bool,
    #[structopt(long, default_value = "16", help = "size of ntopo node table")]
    nodes: u32,
    #[structopt(long, default_value = "32", help = "size of ntopo edge table")]
    edges: u32,
    #[structopt(short, long, help = "Verbose debug output")]
    verbose: bool,
}

fn update_pids_map(map: &mut libbpf_rs::Map, pid: u32) {
    let val = [0; std::mem::size_of::<pid_info>()];
    map.update(&pid.to_ne_bytes(), &val, libbpf_rs::MapFlags::ANY)
        .expect("Failed to update pids map");
}

fn bump_memlock_rlimit() -> Result<()> {
    let rlimit = libc::rlimit {
        rlim_cur: libc::RLIM_INFINITY,
        rlim_max: libc::RLIM_INFINITY,
    };

    if unsafe { libc::setrlimit(libc::RLIMIT_MEMLOCK, &rlimit) } != 0 {
        bail!("Failed to increase rlimit");
    }

    Ok(())
}

fn main() {
    bump_memlock_rlimit().unwrap();
    let opts = Command::from_args();

    let mut builder = NtopoSkelBuilder::default();
    builder.obj_builder.debug(opts.verbose);

    let mut open_skel = builder.open().unwrap();
    open_skel
        .maps_mut()
        .edges()
        .set_max_entries(opts.edges)
        .expect("failed to set edge table size");
    open_skel
        .maps_mut()
        .nodes()
        .set_max_entries(opts.nodes)
        .expect("failed to set node table size");
    let mut skel = open_skel.load().expect("failed to load ntopo");

    for pid in &opts.pids {
        update_pids_map(skel.maps_mut().pids(), *pid);
    }

    let pidsmap = libbpf_rs::MapHandle::try_clone(skel.maps().pids()).unwrap();
    let nodesmap = libbpf_rs::MapHandle::try_clone(skel.maps().nodes()).unwrap();
    let edgesmap = libbpf_rs::MapHandle::try_clone(skel.maps().edges()).unwrap();

    skel.attach().unwrap();

    let pidsmap_for_thread = libbpf_rs::MapHandle::try_clone(skel.maps().pids()).unwrap();

    let _ = std::thread::spawn(move || {
        unix_sock_recv(&pidsmap_for_thread, "/var/ntopo");
    });

    let local_ip = local_ip().unwrap().to_string();

    println!("This is my local IP address: {:?}", local_ip);

    let mut pids = Pids::default();

    loop {
        std::thread::sleep(std::time::Duration::from_secs(30));

        pids.update(&pidsmap);

        let mut nodes = get_nodes(&local_ip, &nodesmap);
        let edges = get_edges(&local_ip, &edgesmap);

        let mut data = vec![];

        for n in &mut nodes {
            n.set_pid_info(&pids);
        }

        for n in &nodes {
            data.push(n.to_line_protocol());
        }

        for e in &edges {
            data.push(e.to_line_protocol());
        }

        let data_string = data.join("\n");
        unix_sock_send("/var/sysom/outline", &data_string);
    }
}
