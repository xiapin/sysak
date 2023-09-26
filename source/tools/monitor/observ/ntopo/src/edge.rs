use std::net::Ipv4Addr;

use crate::edge_info_key;
use libbpf_rs::MapHandle;

use crate::node::AppKind;

pub struct Edge {
    pub client_ip: String,
    pub server_ip: String,
    pub app: AppKind,
    pub request_bytes: u64,
    pub response_bytes: u64,
}

impl Edge {
    pub fn to_line_protocol(&self) -> String {
        format!(
            "sysom_metrics_ntopo_edge,ClientIp={},ServerIp={},LinkId={}{},ReqBytes={},RespBytes={},APP={} Value=1",
            self.client_ip,
            self.server_ip,
            self.client_ip,
            self.server_ip,
            self.request_bytes,
            self.response_bytes,
            self.app,
        )
    }

    pub fn from_key_and_value(_: &String, key_bytes: &Vec<u8>, _: &Vec<u8>) -> Self {
        let (head, body, _tail) = unsafe { key_bytes.align_to::<edge_info_key>() };
        assert!(head.is_empty(), "Data was not aligned");
        let key = &body[0];

        let client_ip = Ipv4Addr::from(u32::from_be(key.saddr)).to_string();
        let server_ip = Ipv4Addr::from(u32::from_be(key.daddr)).to_string();

        Edge {
            client_ip,
            server_ip,
            app: AppKind::Mysql,
            request_bytes: 0,
            response_bytes: 0,
        }
    }
}

pub fn get_edges(local_ip: &String, map: &MapHandle) -> Vec<Edge> {
    let mut res = vec![];
    for key in map.keys() {
        if let Some(val) = map
            .lookup(&key, libbpf_rs::MapFlags::ANY)
            .expect("failed to lookup edges map")
        {
            res.push(Edge::from_key_and_value(local_ip, &key, &val));

            map.delete(&key).expect("failed to delete edges map");
        }
    }

    res
}
