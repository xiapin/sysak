include!(concat!(env!("OUT_DIR"), "/ntopo.skel.rs"));
include!(concat!(env!("OUT_DIR"), "/ntopo.rs"));

use libbpf_rs::MapHandle;
use std::net::Ipv4Addr;

pub use NtopoSkelBuilder;

use crate::nginx::nginx::Nginx;

#[derive(Debug, Default)]
pub struct Node {
    ip: String,
    pid: u32,

    in_bytes: usize,
    out_bytes: usize,

    max_rt: u32,
}

impl Node {
    pub fn set_max_rt(&mut self, max_rt: u32) {
        self.max_rt = max_rt;
    }

    pub fn set_pid(&mut self, pid: u32) {
        self.pid = pid;
    }
}

impl ToString for Node {
    fn to_string(&self) -> String {
        format!(
            "sysom_metrics_ntopo_node,Ip={},Kind=Node,Pid={},Comm=Nginx,Title=Node({}),Icon=NGINX,APP=NGINX,InBytes={},OutBytes={},MaxRT={} Value=1",
            self.ip, self.pid, self.ip, self.in_bytes, self.out_bytes, self.max_rt,
        )
    }
}

pub fn get_nodes(map: &MapHandle) -> Vec<Node> {
    let mut res = vec![];
    for key_bytes in map.keys() {
        if let Some(val) = map
            .lookup(&key_bytes, libbpf_rs::MapFlags::ANY)
            .expect("failed to lookup pid map")
        {
            let (head, body, _tail) = unsafe { key_bytes.align_to::<node_info_key>() };
            assert!(head.is_empty(), "Data was not aligned");
            let key = &body[0];
            let mut node = Node::default();
            node.ip = Ipv4Addr::from(u32::from_be(key.addr)).to_string();
            res.push(node);
            map.delete(&key_bytes).expect("failed to delete nodes map");
        }
    }

    res
}

pub struct Edge {
    pub client_ip: String,
    pub server_ip: String,
}

impl ToString for Edge {
    fn to_string(&self) -> String {
        format!(
            "sysom_metrics_ntopo_edge,ClientIp={},ServerIp={},LinkId={}{},APP=NGINX Value=1",
            self.client_ip, self.server_ip, self.client_ip, self.server_ip,
        )
    }
}

pub fn get_edges(map: &MapHandle) -> Vec<Edge> {
    let mut res = vec![];
    for key_bytes in map.keys() {
        if let Some(val) = map
            .lookup(&key_bytes, libbpf_rs::MapFlags::ANY)
            .expect("failed to lookup edges map")
        {
            let (head, body, _tail) = unsafe { key_bytes.align_to::<edge_info_key>() };
            assert!(head.is_empty(), "Data was not aligned");
            let key = &body[0];
            res.push(Edge {
                client_ip: Ipv4Addr::from(u32::from_be(key.saddr)).to_string(),
                server_ip: Ipv4Addr::from(u32::from_be(key.daddr)).to_string(),
            });

            map.delete(&key_bytes).expect("failed to delete edges map");
        }
    }

    res
}

#[derive(Debug, Default)]
pub struct Pid {
    pub pid: u32,
    pub in_bytes: usize,
    pub out_bytes: usize,
}

fn get_pids(map: &MapHandle) -> Vec<Pid> {
    let mut res = vec![];
    let zero_pid_info = [0_u8; std::mem::size_of::<pid_info>()];
    for key_bytes in map.keys() {
        if let Some(val_bytes) = map
            .lookup(&key_bytes, libbpf_rs::MapFlags::ANY)
            .expect("failed to lookup pid map")
        {
            let (head, body, _tail) = unsafe { val_bytes.align_to::<pid_info>() };
            assert!(head.is_empty(), "Data was not aligned");
            let info = &body[0];

            let pid_arr = [key_bytes[0], key_bytes[1], key_bytes[2], key_bytes[3]];
            let pid_num = u32::from_ne_bytes(pid_arr);
            let pid = Pid {
                pid: pid_num,
                in_bytes: info.in_bytes as usize,
                out_bytes: info.out_bytes as usize,
            };

            res.push(pid);

            map.update(&key_bytes, &zero_pid_info, libbpf_rs::MapFlags::EXIST)
                .expect("failed to update pid map");
        }
    }
    res
}

pub struct NTopo {
    nodes: Vec<Node>,
    edges: Vec<Edge>,
    pids: Vec<Pid>,

    pid: MapHandle,
    node: MapHandle,
    edge: MapHandle,
}

impl NTopo {
    pub fn new(pid: MapHandle, node: MapHandle, edge: MapHandle, nginx: &Nginx) -> Self {
        let mut ntopo = NTopo {
            nodes: Default::default(),
            edges: Default::default(),
            pids: Default::default(),

            pid,
            node,
            edge,
        };

        let zero_pid_info = [0_u8; std::mem::size_of::<pid_info>()];
        for (pid, _) in &nginx.prcs {
            ntopo
                .pid
                .update(&pid.to_ne_bytes(), &zero_pid_info, libbpf_rs::MapFlags::ANY)
                .unwrap();
        }

        ntopo
    }

    fn refresh(&mut self, nginx: &Nginx) {
        let mut edges = get_edges(&self.edge);
        let mut nodes = get_nodes(&self.node);
        let mut pids = get_pids(&self.pid);
        assert!(nodes.len() <= 1);

        let mut total_in_bytes = 0;
        let mut total_out_bytes = 0;
        for pid in &pids {
            total_in_bytes += pid.in_bytes;
            total_out_bytes += pid.out_bytes;
        }

        if nodes.len() == 1 {
            nodes[0].set_max_rt(nginx.access_log.metrics.max_request_time as u32);
            nodes[0].set_pid(nginx.master as u32);
            nodes[0].in_bytes = total_in_bytes;
            nodes[0].out_bytes = total_out_bytes;
        }

        self.nodes = nodes;
        self.edges = edges;
        self.pids = pids;
    }

    pub fn metrics(&mut self, nginx: &Nginx) -> String {
        let mut res = vec![];
        self.refresh(nginx);
        for n in &self.nodes {
            res.push(n.to_string());
        }

        for e in &self.edges {
            res.push(e.to_string());
        }

        let master = nginx.master;
        for p in &self.pids {
            res.push(format!(
                "sysom_nginx_worker_metrics,masterPid={},pid={} inBytes={},outBytes={}",
                master, p.pid, p.in_bytes, p.out_bytes
            ));
        }

        res.join("\n")
    }
}
