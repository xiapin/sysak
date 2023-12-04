use crate::common::asset::Asset;
use anyhow::bail;
use anyhow::Result;
use cached::proc_macro::once;
use once_cell::sync::Lazy;
use rust_embed::RustEmbed;
use std::ffi::CString;
use std::fs;
use std::fs::File;
use std::io::Write;
use std::net::IpAddr;
use std::net::Ipv4Addr;
use std::net::SocketAddr;
use std::net::TcpListener;
use sysctl::Sysctl;
use tempfile::NamedTempFile;

pub static SYSAK_BTF_PATH: Lazy<Option<CString>> = Lazy::new(|| {
    if let Ok(sysak) = std::env::var("SYSAK_WORK_PATH") {
        if let Ok(info) = uname::uname() {
            return Some(
                CString::new(format!("{}/tools/vmlinux-{}", sysak, info.release)).unwrap(),
            );
        }
    }
    None
});

/// Parse the input string and return the IP and port
pub fn parse_ip_str(ip: &str) -> (u32, u16) {
    let vs: Vec<&str> = ip.split(':').collect();
    let mut addr: u32 = 0;
    let mut port: u16 = 0;
    for v in vs {
        if v.contains(".") {
            let ip: Ipv4Addr = v.parse().unwrap();
            addr = ip.into();
            addr = addr.to_be();
        } else {
            port = v.parse().unwrap();
        }
    }
    return (addr, port);
}

/// Check whether rps is enabled
pub fn detect_rps() -> bool {
    if let Ok(ctl) = sysctl::Ctl::new("net.core.rps_sock_flow_entries") {
        if let Ok(val) = ctl.value_string() {
            return !(val.cmp(&"0".to_owned()) == std::cmp::Ordering::Equal);
        }
    }
    false
}

/// Handling Perf buffer loss events
pub fn handle_lost_events(cpu: i32, count: u64) {
    eprintln!("Lost {count} events on CPU {cpu}");
}

/// convert any structure into bytes
pub unsafe fn any_as_u8_slice<T: Sized>(p: &T) -> &[u8] {
    ::core::slice::from_raw_parts((p as *const T) as *const u8, ::core::mem::size_of::<T>())
}

pub fn btf_path_ptr() -> *const i8 {
    SYSAK_BTF_PATH
        .as_ref()
        .map_or(std::ptr::null(), |x| x.as_ptr())
}

pub fn is_virtio_net(interface: &str) -> bool {
    let driver_path = format!("/sys/class/net/{}/device/driver", interface);

    if let Ok(driver_link) = fs::read_link(driver_path) {
        if let Some(driver_name) = driver_link.file_name() {
            if let Some(driver) = driver_name.to_str() {
                return driver == "virtio_net";
            }
        }
    }

    false
}

pub fn get_queue_count(interface: &str) -> Option<usize> {
    let queues_dir = format!("/sys/class/net/{}/queues", interface);

    if let Ok(entries) = fs::read_dir(queues_dir) {
        let count = entries.count();
        Some(count)
    } else {
        None
    }
}

pub fn get_current_kernel_version() -> Result<String> {
    let mut info = unsafe { std::mem::MaybeUninit::<libc::utsname>::zeroed().assume_init() };
    let mut release_version = Vec::with_capacity(info.release.len());
    let ret = unsafe { libc::uname(&mut info as *mut libc::utsname) };
    if ret < 0 {
        bail!("failed to call function: libc::uname, error code: {}", ret)
    }

    for i in info.release {
        release_version.push(i as u8);
    }

    Ok(String::from_utf8(release_version)?
        .trim_matches(char::from(0))
        .to_owned())
}

pub fn get_send_receive_queue() -> Result<(usize, usize)> {
    if let Some(file_data) = Asset::get("rtrace.db") {
        let mut temp_file = NamedTempFile::new()?;
        temp_file.write_all(&file_data.data)?;
        let path = temp_file.into_temp_path();
        let conn = sqlite::open(path)?;
        let query = format!(
            "SELECT * FROM sql_count WHERE version = \"{}\"",
            get_current_kernel_version()?
        );

        let mut stmt = conn.prepare(&query)?;

        while let Ok(sqlite::State::Row) = stmt.next() {
            let sq = stmt.read::<i64, _>("send_queue")? as usize;
            let rq = stmt.read::<i64, _>("receive_queue")? as usize;
            return Ok((sq, rq));
        }
    }

    bail!("not found send_queue and receive_queue in rtrace.db")
}

/// Monotonically increasing timestamp, incremented by 1 when the clock interrupt
/// is triggered. This clock source is used by the bpf_ktime_get_ns function.
pub fn current_monotime() -> u64 {
    let mut ts = libc::timespec {
        tv_sec: 0,
        tv_nsec: 0,
    };
    unsafe { libc::clock_gettime(libc::CLOCK_MONOTONIC, &mut ts) };

    (ts.tv_sec as u64) * 1000_000_000 + (ts.tv_nsec as u64)
}

/// System-wide realtime clock. It is generally synchronized with the clock of
/// the master server through the ntp protocol.
pub fn current_realtime() -> u64 {
    let mut ts = libc::timespec {
        tv_sec: 0,
        tv_nsec: 0,
    };
    unsafe { libc::clock_gettime(libc::CLOCK_REALTIME, &mut ts) };

    (ts.tv_sec as u64) * 1000_000_000 + (ts.tv_nsec as u64)
}

#[once]
pub fn get_host_ip() -> std::net::IpAddr {
    local_ip_address::local_ip().unwrap()
}

#[once]
pub fn get_host_ipv4() -> std::net::Ipv4Addr {
    match local_ip_address::local_ip().unwrap() {
        IpAddr::V4(v4) => v4,
        IpAddr::V6(_) => panic!("ipv6 not support"),
    }
}

pub fn ns2ms(ns: u64) -> f32 {
    (ns as f32) / 1_000_000.0
}

pub fn allocate_port(sport: u16) -> u16 {
    let addr: SocketAddr = format!("127.0.0.1:{}", sport)
        .parse()
        .expect("Invalid address");
    if let Ok(listener) = TcpListener::bind(addr) {
        let local_addr = listener.local_addr().expect("Failed to get local address");
        let port = local_addr.port();
        drop(listener);
        port
    } else {
        0
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parse_ip() {
        let test1 = "127.0.0.1";
        let test2 = "127.0.0.1:8080";
        let test3 = "8080";

        assert_eq!(parse_ip_str(test1), (16777343, 0));
        assert_eq!(parse_ip_str(test2), (16777343, 8080));
        assert_eq!(parse_ip_str(test3), (0, 8080));
    }

    #[test]
    fn virtio_net() {
        assert!(is_virtio_net("eth0"));
        assert!(!is_virtio_net("lo"));
    }

    #[test]
    fn virtio_queue_count() {
        assert_ne!(get_queue_count("eth0").unwrap(), 0);
    }

    #[test]
    fn kernel_version() {
        assert!(get_current_kernel_version().is_ok())
    }

    #[test]
    fn send_receive_queue() {
        assert!(get_send_receive_queue().is_ok());
    }
}
