use std::{
    collections::{HashMap, HashSet},
    process::Command,
};

use anyhow::Result;
use procfs::{
    process::{Process, Stat},
    Current, Meminfo,
};

use crate::nginx::status::Status;
use crate::{cpu::Cpu, memory::Memory};

use super::access_log::AccessLog;

pub struct Nginx {
    pub prcs: HashMap<i32, Process>,
    pub stats: HashMap<i32, (Stat, Stat)>,
    pub ppids: HashMap<i32, i32>,

    pub master: i32,
    workers: HashSet<i32>,

    meminfo: Meminfo,
    info: NginxInfo,

    status: Status,
    pub access_log: AccessLog,
}

impl Nginx {
    pub fn new(master: i32, workers: HashSet<i32>, prcs: HashMap<i32, Process>) -> Self {
        let mut nginx = Nginx {
            prcs,
            stats: Default::default(),
            ppids: Default::default(),
            meminfo: Meminfo::current().unwrap(),
            info: NginxInfo::new(master),
            master,
            workers,
            status: Status::new("http://127.0.0.1/api".to_owned()),
            access_log: AccessLog::new(master),
        };
        nginx.refresh();

        nginx
    }

    fn refresh(&mut self) {
        self.ppids.clear();
        for (pid, prc) in &self.prcs {
            match prc.stat() {
                Ok(stat) => {
                    let ppid = stat.ppid;
                    self.stats
                        .entry(*pid)
                        .and_modify(|(x, y)| {
                            std::mem::swap(x, y);
                            *y = stat.clone();
                        })
                        .or_insert((stat.clone(), stat));
                    self.ppids.entry(ppid).and_modify(|c| *c += 1).or_insert(1);
                }
                Err(e) => {
                    log::error!("failed to read process stat, error messsage: {}", e);
                }
            }
        }
    }

    pub fn metrics(&mut self) -> String {
        self.refresh();
        let mut metrics = vec![];

        metrics.push(self.nginx_main_metrics());
        metrics.push(self.nginx_process_metrics());

        metrics.join("\n")
    }

    fn nginx_main_metrics(&mut self) -> String {
        format!(
            "sysom_nginx_main_metrics,masterPid={} errorLog=0,workersCount={},{},{}",
            self.master,
            self.workers.len(),
            if let Some(x) = self.status.metrics() {
                x
            } else {
                "noStatus=1".to_owned()
            },
            self.access_log.metrics(),
        )
    }

    fn nginx_process_metrics(&mut self) -> String {
        let mut metrics = vec![];

        for (pid, (last, now)) in &self.stats {
            let metric = format!(
                "sysom_nginx_worker_metrics,masterPid={},pid={} {},{}",
                self.master,
                pid,
                Cpu::new(last, now).to_string(),
                Memory::new(now, &self.meminfo).to_string()
            );
            metrics.push(metric);
        }
        metrics.join("\n")
    }
}

pub struct NginxMetrics {
    pid: i32,
    master_pid: i32,
    cpu: Cpu,
    mem: Memory,
}

impl NginxMetrics {
    pub fn to_line_protocol(&self) -> String {
        format!(
            "sysom_nginx_metrics,masterPid={},pid={} {},{}",
            self.master_pid,
            self.pid,
            self.cpu.to_string(),
            self.mem.to_string()
        )
    }

    pub fn is_master(&self) -> bool {
        self.pid == self.master_pid
    }
}

pub fn find_nginx_instances() -> Vec<Nginx> {
    let mut res = vec![];
    let mut prcs = find_nginx_processes();
    assert_ne!(prcs.len(), 0, "No running nginx instance");

    let nginxes = classify_processes(&prcs);
    for (master, workers) in nginxes {
        let mut nginx_prcs = HashMap::from([(master, prcs.remove(&master).unwrap())]);
        for worker in &workers {
            nginx_prcs.insert(*worker, prcs.remove(worker).unwrap());
        }
        res.push(Nginx::new(master, workers, nginx_prcs));
    }
    res
}

fn find_nginx_processes() -> HashMap<i32, Process> {
    let mut prcs = HashMap::new();
    for prc in procfs::process::all_processes().unwrap() {
        let prc = prc.unwrap();
        if let Ok(stat) = prc.stat() {
            if stat.comm.starts_with("nginx") {
                log::debug!("find nginx process: {:?}", prc);
                prcs.insert(prc.pid, prc);
            }
        }
    }
    prcs
}

// master workers
fn classify_processes(prcs: &HashMap<i32, Process>) -> HashMap<i32, HashSet<i32>> {
    let mut ptree: HashMap<i32, HashSet<i32>> = HashMap::new();
    for (pid, prc) in prcs {
        if let Ok(stat) = prc.stat() {
            let ppid = stat.ppid;

            if !prcs.contains_key(&ppid) {
                continue;
            }

            let mut entry = ptree.entry(ppid).or_insert(HashSet::new());
            entry.insert(*pid);
        }
    }
    ptree
}

#[derive(Debug, Default)]
struct NginxInfo {
    args: HashMap<String, String>,
}

impl From<String> for NginxInfo {
    fn from(value: String) -> Self {
        let lines = value.lines().map(str::trim);
        let mut args = Default::default();
        for line in lines {
            match line {
                line if line.starts_with("configure arguments") => {
                    args = NginxInfo::parse_configure_arguments(line);
                }
                _ => {}
            }
        }

        NginxInfo { args }
    }
}

impl NginxInfo {
    pub fn new(pid: i32) -> Self {
        let exe = format!("/proc/{}/exe", pid);
        let output = Command::new(&exe)
            .arg("-V")
            .output()
            .map_err(|e| log::error!("nginx -V failed: {}", e))
            .ok()
            .unwrap();
        let outbuf = String::from_utf8_lossy(&output.stdout).to_string();
        NginxInfo::from(outbuf)
    }

    pub fn conf_path(&self) -> Option<&String> {
        self.args.get("conf-path")
    }

    pub fn access_log_path(&self) -> Option<&String> {
        self.args.get("http-log-path")
    }

    pub fn error_log_path(&self) -> Option<&String> {
        self.args.get("error-log-path")
    }

    fn parse_configure_arguments(line: &str) -> HashMap<String, String> {
        let line = line.trim_start_matches("configure arguments:");
        let flags: Vec<&str> = line.split(" --").collect();
        let mut result: HashMap<String, String> = HashMap::new();
        for flag in flags {
            let vals: Vec<&str> = flag.split("=").collect();
            match vals.len() {
                1 => {
                    if !vals[0].is_empty() {
                        result.insert(vals[0].to_string(), String::from("true"));
                    }
                }
                2 => {
                    result.insert(vals[0].to_string(), vals[1].to_string());
                }
                _ => {}
            }
        }
        result
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn nginx_info() {
        let nginx_v = r#"nginx version: nginx/1.20.1
        built by gcc 10.2.1 20200825 (Alibaba 10.2.1-3 2.32) (GCC) 
        built with OpenSSL 1.1.1k  FIPS 25 Mar 2021
        TLS SNI support enabled
        configure arguments: --prefix=/usr/share/nginx --sbin-path=/usr/sbin/nginx --modules-path=/usr/lib64/nginx/modules --conf-path=/etc/nginx/nginx.conf --error-log-path=/var/log/nginx/error.log --http-log-path=/var/log/nginx/access.log --http-client-body-temp-path=/var/lib/nginx/tmp/client_body --http-proxy-temp-path=/var/lib/nginx/tmp/proxy --http-fastcgi-temp-path=/var/lib/nginx/tmp/fastcgi --http-uwsgi-temp-path=/var/lib/nginx/tmp/uwsgi --http-scgi-temp-path=/var/lib/nginx/tmp/scgi --pid-path=/run/nginx.pid --lock-path=/run/lock/subsys/nginx --user=nginx --group=nginx --with-file-aio --with-ipv6 --with-http_ssl_module --with-http_v2_module --with-http_realip_module --with-stream_ssl_preread_module --with-http_addition_module --with-http_xslt_module=dynamic --with-http_image_filter_module=dynamic --with-http_sub_module --with-http_dav_module --with-http_flv_module --with-http_mp4_module --with-http_gunzip_module --with-http_gzip_static_module --with-http_random_index_module --with-http_secure_link_module --with-http_degradation_module --with-http_slice_module --with-http_stub_status_module --with-http_perl_module=dynamic --with-http_auth_request_module --with-mail=dynamic --with-mail_ssl_module --with-pcre --with-pcre-jit --with-stream=dynamic --with-stream_ssl_module --with-debug --with-cc-opt='-O2 -g -pipe -Wall -Werror=format-security -Wp,-D_FORTIFY_SOURCE=2 -Wp,-D_GLIBCXX_ASSERTIONS -fexceptions -fstack-protector-strong -grecord-gcc-switches -specs=/usr/lib/rpm/redhat/redhat-hardened-cc1 -specs=/usr/lib/rpm/redhat/redhat-annobin-cc1 -m64 -mtune=generic -fasynchronous-unwind-tables -fstack-clash-protection -fcf-protection -floop-unroll-and-jam -ftree-loop-distribution --param early-inlining-insns=160 --param inline-heuristics-hint-percent=800 --param inline-min-speedup=50 --param inline-unit-growth=256 --param max-average-unrolled-insns=500 --param max-completely-peel-times=32 --param max-completely-peeled-insns=800 --param max-inline-insns-auto=128 --param max-inline-insns-small=128 --param max-unroll-times=16 --param max-unrolled-insns=16 -O3' --with-compat --with-ld-opt='-Wl,-z,relro -Wl,-z,now -specs=/usr/lib/rpm/redhat/redhat-hardened-ld -Wl,-E'"#;
        let info = NginxInfo::from(nginx_v.to_owned());
        assert_eq!(info.conf_path(), Some(&"/etc/nginx/nginx.conf".to_owned()));
        assert_eq!(
            info.access_log_path(),
            Some(&"/var/log/nginx/access.log".to_owned())
        );
        assert_eq!(
            info.error_log_path(),
            Some(&"/var/log/nginx/error.log".to_owned())
        );
    }
}
