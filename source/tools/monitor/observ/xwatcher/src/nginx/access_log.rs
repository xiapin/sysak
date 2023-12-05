use crate::rtrace::xtrace_collect;
use crate::utils::send_alarm;
use anyhow::{bail, Result};
use chrono::prelude::*;
use once_cell::sync::Lazy;
use rtrace::common::utils::current_monotime;
use rtrace::common::utils::parse_ip_str;
use serde::ser::SerializeStruct;
use serde::Serialize;
use serde::Serializer;
use std::net::Ipv4Addr;
use std::sync::Mutex;
use tokio::runtime::Runtime;

static GLOBAL_METRICS: Lazy<Mutex<AccessLogMetrics>> = Lazy::new(|| {
    let metrics = AccessLogMetrics::default();
    Mutex::new(metrics)
});

#[derive(Debug)]
pub struct AccessLog {
    master: i32,
    pub metrics: AccessLogMetrics,
}

impl AccessLog {
    pub fn new(master: i32) -> Self {
        std::thread::spawn(move || {
            let rt = Runtime::new().unwrap();
            rt.block_on(async {
                let mut lines = linemux::MuxedLines::new().unwrap();
                lines.add_file("/var/log/nginx/access.log").await.unwrap();
                while let Ok(Some(line)) = lines.next_line().await {
                    match parse_accesslog_entry(line.line()) {
                        Ok(entry) => {
                            send_accesslog_alarm(&entry, master);
                            GLOBAL_METRICS.lock().unwrap().add_entry(entry);
                        }
                        Err(_) => log::error!("failed to parse access log"),
                    }
                }
            });
        });

        AccessLog {
            master,
            metrics: Default::default(),
        }
    }

    pub fn metrics(&mut self) -> String {
        self.metrics = GLOBAL_METRICS.lock().unwrap().clone();
        GLOBAL_METRICS.lock().unwrap().reset();
        self.metrics.to_string()
    }
}

fn send_accesslog_alarm(entry: &AccessLogEntry, master: i32) {
    let mut valid = false;
    if entry.status.eq("499") || entry.status.eq("504") {
        valid = true;
    }
    if entry.request_time > 10 {
        valid = true;
    }

    if valid {
        let raddr: Ipv4Addr = entry.remote_addr.parse().unwrap();
        let (addr, uport) = parse_ip_str(&entry.upstream_addr);
        let uaddr = Ipv4Addr::from(u32::from_be(addr));
        let r = current_monotime();
        let l = r - (entry.request_time as u64) * 1_000_000;
        let mut reason = xtrace_collect(raddr, uaddr, uport, (l, r));
        if reason.len() < 3 {
            reason = "未发现网络丢包和重传".to_owned();
        }

        let me = MetricEvent::new(master.to_string(), entry, reason);
        send_alarm(me);
    }
}

#[derive(Debug)]
pub struct AccessLogEntry {
    request_time: usize,
    upstream_response_time: usize,
    status: String,
    remote_addr: String,
    upstream_addr: String,
    request: String,
}

fn parse_until_space(input: &str) -> Result<(&str, &str)> {
    let res = input.split_once(" ");
    match res {
        Some(r) => Ok(r),
        None => bail!("failed to parse"),
    }
}

fn parse_inside_quotes(input: &str) -> Result<(&str, &str)> {
    if let Some(start) = input.find('"') {
        if let Some(end) = input[start + 1..].find('"') {
            return Ok((
                &input[start + 1..start + end + 1],
                &input[start + end + 1..],
            ));
        }
    }
    bail!("failed to find quotes")
}

fn parse_time_ms(time: &str) -> Result<usize> {
    let f: f32 = time.parse()?;
    Ok((f * 1000.0) as usize)
}

fn parse_accesslog_entry(input: &str) -> Result<AccessLogEntry> {
    let (tmp, input) = parse_until_space(input)?;
    let request_time = parse_time_ms(tmp)?;

    let (tmp, input) = parse_until_space(input)?;
    let upstream_response_time = parse_time_ms(tmp)?;

    let (status, input) = parse_until_space(input)?;
    let (remote_addr, input) = parse_until_space(input)?;
    let (upstream_addr, input) = parse_until_space(input)?;

    let (request, _) = parse_inside_quotes(input)?;

    Ok(AccessLogEntry {
        request_time,
        upstream_response_time,
        status: status.to_owned(),
        remote_addr: remote_addr.to_owned(),
        upstream_addr: upstream_addr.to_owned(),
        request: request.to_owned(),
    })
}

#[derive(Debug, Default, Clone, Copy)]
pub struct AccessLogMetrics {
    pub status: [usize; 5],

    pub request_time_count: usize,
    pub request_time_total: usize,

    pub upstream_response_time_count: usize,
    pub upstream_response_time_total: usize,

    pub max_request_time: usize,
    pub max_upstream_reponse_time: usize,

    pub request_jitter: usize,
}

impl ToString for AccessLogMetrics {
    fn to_string(&self) -> String {
        format!(
            "status_1xx={},status_2xx={},status_3xx={},status_4xx={},status_5xx={},requestTimeAvg={},upstreamTimeAvg={},maxRequestTime={},maxUpstreamTime={},requestJitter={}",
            self.status[0],
            self.status[1],
            self.status[2],
            self.status[3],
            self.status[4],
            if self.request_time_count == 0 { 0} else { self.request_time_total/self.request_time_count},
            if self.upstream_response_time_count==0 {0} else {self.upstream_response_time_total/self.upstream_response_time_count},
            self.max_request_time,
            self.max_upstream_reponse_time,
            self.request_jitter
        )
    }
}

impl AccessLogMetrics {
    fn reset(&mut self) {
        self.status[0] = 0;
        self.status[1] = 0;
        self.status[2] = 0;
        self.status[3] = 0;
        self.status[4] = 0;
        self.request_time_count = 0;
        self.request_time_total = 0;

        self.upstream_response_time_count = 0;
        self.upstream_response_time_total = 0;

        self.max_request_time = 0;
        self.max_upstream_reponse_time = 0;
        self.request_jitter = 0;
    }

    pub fn add_request_time(&mut self, time_ms: usize) {
        self.request_time_total += time_ms;
        self.request_time_count += 1;
        self.max_request_time = std::cmp::max(time_ms, self.max_request_time);
    }

    pub fn add_upstream_response_time(&mut self, time_ms: usize) {
        self.request_time_total += time_ms;
        self.request_time_count += 1;
        self.max_upstream_reponse_time = std::cmp::max(time_ms, self.max_upstream_reponse_time);
    }

    pub fn add_http_status(&mut self, status: &str) {
        if let Some(first_char) = status.chars().next() {
            let index = first_char as usize - '0' as usize;
            self.status[index - 1] += 1;
        } else {
            log::error!("wrong format status code: {}", status);
        }
    }

    pub fn add_entry(&mut self, entry: AccessLogEntry) {
        if !entry.status.eq("499") && !entry.status.eq("504") && entry.request_time > 10{
            self.request_jitter += 1;
        }
        
        self.add_request_time(entry.request_time);
        self.add_upstream_response_time(entry.upstream_response_time);
        self.add_http_status(&entry.status);
    }
}

#[derive(Debug, Clone)]
struct MetricEvent {
    ts: String,
    remote_addr: String,
    upstream_addr: String,
    request: String,
    request_time: String,
    upstream_response_time: String,
    master_pid: String,
    diag_id: String,
    status: String,
    reason: String,
    diag: bool,
}

impl Serialize for MetricEvent {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let mut s = serializer.serialize_struct("MetricEvent", 7)?;
        s.serialize_field("ts", &self.ts)?;
        s.serialize_field("remoteAddr", &self.remote_addr)?;
        s.serialize_field("upstreamAddr", &self.upstream_addr)?;
        s.serialize_field("request", &self.request)?;
        s.serialize_field("requestTime", &self.request_time)?;
        s.serialize_field("upstreamResponseTime", &self.upstream_response_time)?;
        s.serialize_field("masterPid", &self.master_pid)?;
        s.serialize_field("diagId", &self.diag_id)?;
        s.serialize_field("status", &self.status)?;
        s.serialize_field("reason", &self.reason)?;
        s.serialize_field("diag", &self.diag)?;
        s.end()
    }
}

impl MetricEvent {
    pub fn new(pid: String, entry: &AccessLogEntry, reason: String) -> Self {
        let local: DateTime<Local> = Local::now();
        MetricEvent {
            ts: local.format("%d/%m/%Y %H:%M:%S").to_string(),
            remote_addr: entry.remote_addr.clone(),
            upstream_addr: entry.upstream_addr.clone(),
            request: entry.request.clone(),
            request_time: entry.request_time.to_string(),
            upstream_response_time: entry.upstream_response_time.to_string(),
            master_pid: pid,
            diag_id: "none".to_string(),
            status: entry.status.clone(),
            reason,
            diag: true
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parse_accesslog() {
        let text = r#"0.002 0.002 301 140.205.118.62 192.168.0.76:8000 "GET /polls HTTP/1.1""#;
        println!("{:?}", parse_accesslog_entry(text));
    }

    #[test]
    fn show_metric_event() {
        let text = r#"0.002 0.002 301 140.205.118.62 192.168.0.76:8000 "GET /polls HTTP/1.1""#;
        let entry = parse_accesslog_entry(text).unwrap();
        let me = MetricEvent::new(12.to_string(), &entry);

        println!("{}", serde_json::to_string(&me).unwrap());
    }
}
