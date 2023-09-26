use libbpf_rs::MapHandle;

use crate::node_info;
use crate::node_info_key;
use crate::pid::Pids;
use std::cmp::Ordering;
use std::fmt;
use std::net::Ipv4Addr;

pub enum NodeKind {
    Pod,
    Node,
}

impl fmt::Display for NodeKind {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let s = match self {
            NodeKind::Pod => "Pod",
            NodeKind::Node => "Node",
        };
        write!(f, "{s}")
    }
}

pub enum AppKind {
    Other,
    Mysql,
}

impl fmt::Display for AppKind {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let s = match self {
            AppKind::Other => "OTHER",
            AppKind::Mysql => "MYSQL",
        };
        write!(f, "{s}")
    }
}

pub enum IconKind {
    Node,
    Mysql,
}

impl fmt::Display for IconKind {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let s = match self {
            IconKind::Node => "NODE",
            IconKind::Mysql => "MYSQL",
        };
        write!(f, "{s}")
    }
}

pub struct Node {
    ip: String,
    kind: NodeKind,
    pod: String,
    pid: u32,
    comm: String,
    poduuid: String,
    container_id: String,
    namespace: String,
    app: AppKind,
    title: String,
    icon: IconKind,

    in_bytes: i64,
    out_bytes: i64,

    server_ip: String,
    client_max_rt: u32,
    client_avg_rt: u32,
    client_min_rt: u32,

    client_ip: String,
    server_max_rt: u32,
    server_avg_rt: u32,
    server_min_rt: u32,
    sport: u16,
    dport: u16,
    requests: u32,
}

impl Node {
    pub fn to_line_protocol(&self) -> String {
        format!(
            "sysom_metrics_ntopo_node,Ip={},Kind={},Pod={},Pid={},Comm={},Title={},Icon={},PodUUID={},ContainerID={},NameSpace={},APP={},InBytes={},OutBytes={},MaxRT={},Connection={},AvgRT={},Requests={} Value=1",
            self.ip,
            self.kind,
            self.pod,
            self.pid,
            self.comm,
            if let NodeKind::Pod = self.kind {
                self.pod.clone()
            } else {
                format!("Node({})", self.ip)
            },
            self.icon,
            self.poduuid,
            self.container_id,
            self.namespace,
            self.app,
            self.in_bytes,
            self.out_bytes,
            std::cmp::max(self.client_max_rt, self.server_max_rt),
            format!("{}:{}->{}:{}", self.client_ip, self.sport, self.server_ip, self.dport),
            std::cmp::max(self.client_avg_rt, self.server_avg_rt),
            self.requests,
        )
    }

    pub fn set_pid_info(&mut self, pids: &Pids) {
        if let Some(pi) = pids.pids.get(&self.pid) {
            if pi.container_id.len() > 12 {
                self.container_id = pi.container_id[0..12].to_owned();
            } else {
                self.container_id = pi.container_id.clone();
            }
            self.comm = pi.comm.clone();
        }
    }

    pub fn from_key_and_value(local_ip: &String, key_bytes: &Vec<u8>, val_bytes: &Vec<u8>) -> Self {
        let (head, body, _tail) = unsafe { key_bytes.align_to::<node_info_key>() };
        assert!(head.is_empty(), "Data was not aligned");
        let key = &body[0];

        let (head, body, _tail) = unsafe { val_bytes.align_to::<node_info>() };
        assert!(head.is_empty(), "Data was not aligned");
        let info = &body[0];

        let ip = Ipv4Addr::from(u32::from_be(key.addr)).to_string();
        let kind;
        if ip.cmp(local_ip) == Ordering::Equal {
            kind = NodeKind::Node;
        } else {
            kind = NodeKind::Pod;
        }

        Node {
            ip,
            kind,
            pid: info.pid,
            comm: Default::default(),
            pod: Default::default(),
            poduuid: Default::default(),
            container_id: Default::default(),
            namespace: Default::default(),
            app: AppKind::Mysql,
            title: Default::default(),
            icon: IconKind::Mysql,

            in_bytes: 0,
            out_bytes: 0,

            server_ip: Ipv4Addr::from(u32::from_be(info.server_addr)).to_string(),
            client_max_rt: info.client_max_rt_us,
            client_avg_rt: if info.client_tot_rt_hz == 0 {
                0
            } else {
                info.client_tot_rt_us / info.client_tot_rt_hz
            },
            client_min_rt: info.client_min_rt_us,

            client_ip: Ipv4Addr::from(u32::from_be(info.client_addr)).to_string(),
            server_max_rt: info.server_max_rt_us,
            server_avg_rt: if info.server_tot_rt_hz == 0 {
                0
            } else {
                info.server_tot_rt_us / info.server_tot_rt_hz
            },
            server_min_rt: info.server_min_rt_us,
            sport: info.sport,
            dport: info.dport,
            requests: info.requests,
        }
    }
}

pub fn get_nodes(local_ip: &String, map: &MapHandle) -> Vec<Node> {
    let mut res = vec![];
    for key in map.keys() {
        if let Some(val) = map
            .lookup(&key, libbpf_rs::MapFlags::ANY)
            .expect("failed to lookup pid map")
        {
            res.push(Node::from_key_and_value(local_ip, &key, &val));
            map.delete(&key).expect("failed to delete nodes map");
        }
    }

    res
}
