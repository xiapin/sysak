use std::{os::unix, net::Ipv4Addr};

use cached::proc_macro::once;
use chrono::prelude::*;
use procfs::process::Process;
use serde::Serialize;

pub const METRICS_PERIOD: i32 = 30;

#[once]
pub fn cached_clock_tick() -> i64 {
    unsafe { libc::sysconf(libc::_SC_CLK_TCK) }
}
#[derive(Debug, Clone, Serialize)]
struct Alarm<T>
where
    T: Serialize,
{
    alert_id: String,
    instance: String,
    alert_item: String,
    alert_category: String,
    alert_source_type: String,
    alert_time: i64,
    status: String,
    labels: T,
}

#[derive(Debug, Clone, Serialize)]
struct AlarmTopic<T: Serialize> {
    topic: String,
    data: Alarm<T>,
}

impl<T> Alarm<T>
where
    T: Serialize,
{
    pub fn new(labels: T) -> Self {
        Alarm {
            alert_id: uuid::Uuid::new_v4().to_string(),
            instance: get_host_ip(),
            alert_item: "nginx".to_owned(),
            alert_category: "MONITOR".to_owned(),
            alert_source_type: "sysak".to_owned(),
            alert_time: (Utc::now().timestamp() as i64) * 1000,
            status: "FIRING".to_owned(),
            labels,
        }
    }
}

pub fn send_alarm<T>(labels: T)
where
    T: Serialize,
{
    let alarm = Alarm::new(labels);
    let alarm_topic = AlarmTopic {
        topic: "SYSOM_SAD_ALERT".to_owned(),
        data: alarm,
    };
    log::debug!("{}", serde_json::to_string(&alarm_topic).unwrap());
    let client = reqwest::blocking::Client::new();
    match client
        .post("http://192.168.0.127/api/v1/cec_proxy/proxy/dispatch")
        .json(&alarm_topic)
        .send()
    {
        Ok(resp) => {
            log::debug!("{}", resp.text().unwrap());
        }
        Err(e) => {
            log::error!("failed to send alarm event: {}", e);
        }
    }
}

#[once]
fn get_host_ip() -> String {
    local_ip_address::local_ip().unwrap().to_string()
}

pub fn get_ipv4(s: &str) -> Ipv4Addr {
    let vs: Vec<&str> = s.split(':').collect();
    for v in vs {
        if v.contains(".") {
            let ip: Ipv4Addr = v.parse().unwrap();
            return ip;
        }
    }
    panic!("no ip addr")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn show_hostip() {
        println!("{:?}", get_ipv4("127.0.0.1:600"));
    }
    
}
