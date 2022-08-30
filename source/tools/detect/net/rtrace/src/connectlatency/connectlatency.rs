use crate::utils::macros::*;
ebpf_common_use!(connectlatency);

use std::net::{IpAddr, Ipv4Addr, SocketAddr, SocketAddrV4};

#[derive(Debug, StructOpt)]
pub struct ConnectlatencyCommand {
    #[structopt(long, help = "pid to trace")]
    pid: Option<u32>,
}

pub fn build_connectlatency(
    opts: &ConnectlatencyCommand,
    debug: bool,
    btf: &Option<String>,
) -> Result<()> {
    // load, set filter and attach
    let mut filter = Filter::new();
    if let Some(pid) = opts.pid {
        filter.set_pid(pid);
    }
    let mut skel = Skel::default();

    skel.open_load(debug, btf, vec![], vec![])?;
    skel.filter_map_update(0, unsafe {
        std::mem::transmute::<commonbinding::filter, filter>(filter.filter())
    })?;
    skel.attach()?;

    loop {
        if let Some(data) = skel.poll(std::time::Duration::from_millis(100))? {
            println!("{}", SockMapVal::from_data(data.1));
        }
    }

    bail!("not support yet")
}

enum SockEventType {
    TcpConnect,
    TcpConnectRet,
    TcpRcvSynACK,
}

impl SockEventType {
    pub fn from_u32(val: u32) -> SockEventType {
        match val {
            TCP_CONNECT => SockEventType::TcpConnect,
            TCP_CONNECT_RET => SockEventType::TcpConnectRet,
            TCP_RCV_SYNACK => SockEventType::TcpRcvSynACK,
            _ => panic!("wrong value {}", val),
        }
    }
}

impl fmt::Display for SockEventType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            SockEventType::TcpConnect => write!(f, "Connect"),
            SockEventType::TcpConnectRet => write!(f, "ConnectReturn"),
            SockEventType::TcpRcvSynACK => write!(f, "SynAck"),
        }
    }
}

struct SockMapVal {
    val: sockmap_val,
    data: Vec<u8>,
}

impl SockMapVal {
    pub fn from(val: sockmap_val) -> SockMapVal {
        SockMapVal {
            val,
            data: Vec::default(),
        }
    }

    pub fn from_data(data: Vec<u8>) -> SockMapVal {
        SockMapVal {
            val: unsafe { std::mem::MaybeUninit::zeroed().assume_init() },
            data,
        }
    }
}

impl fmt::Display for SockMapVal {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        //
        unsafe {
            let ptr = &self.data[0] as *const u8 as *const sockmap_val;

            let daddr = (*ptr).ap.daddr;
            let dport = (*ptr).ap.dport;
            let saddr = (*ptr).ap.saddr;
            let sport = (*ptr).ap.sport;

            let remote = SocketAddr::new(IpAddr::V4(Ipv4Addr::from(u32::from_be(daddr))), dport);
            let local = SocketAddr::new(IpAddr::V4(Ipv4Addr::from(u32::from_be(saddr))), sport);
            
            write!(f, "{}/{} ", (*ptr).pid, String::from_utf8_unchecked((*ptr).comm.to_vec()))?;
            write!(f, "{}->{} ", local, remote)?;
            let mut prets = (*ptr).tss[0].ts;
            
            write!(
                f,
                "{} ",
                SockEventType::from_u32((*ptr).tss[0].event.into())
            )?;
            for i in 1..(*ptr).curidx as usize {
                let delta = (*ptr).tss[i].ts - prets;
                prets = (*ptr).tss[i].ts;
                write!(
                    f,
                    "->{}ms-> {} ",
                    delta / 1000_000,
                    SockEventType::from_u32((*ptr).tss[i].event.into())
                )?;
            }
            
            // if (*ptr).ret == 0 {
            //     write!(f, " => Connect Sucessfully")?;
            // } else {
            //     write!(f, " => Connect failed")?;
            // }

            write!(f, "")
        }
    }
}
