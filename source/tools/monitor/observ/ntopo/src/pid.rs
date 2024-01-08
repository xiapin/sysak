use crate::pid_info;
use libbpf_rs::MapHandle;
use std::collections::HashMap;

use regex::Regex;
use std::fs;
use std::io::Error;
use std::path::Path;
use std::process::Command;

const CONTAINER_TYPE_DOCKER: usize = 0;
const CONTAINER_TYPE_CRI_CONTAINERD: usize = 1;
const CONTAINER_TYPE_CRIO: usize = 2;
const CONTAINER_TYPE_K8S_OTHER: usize = 3;
const CONTAINER_TYPE_UNKNOWN: usize = 4;

const CONTAINER_TYPE_STR: [&str; 5] = ["docker", "cri-containerd", "crio", "kubepods", "unknown"];

const CONTAINER_TYPE_REGEX_STR: [&str; 5] = ["docker", "cri-containerd", "crio", "\\S+", "unknown"];

struct RegMatch {
    burstable_path_regex: Regex,
    besteffort_path_regex: Regex,
    guaranteed_path_regex: Regex,
}

impl RegMatch {
    fn is_match(&self, path: &str) -> bool {
        if path.contains("burstable") {
            self.burstable_path_regex.is_match(path)
        } else if path.contains("besteffort") {
            self.besteffort_path_regex.is_match(path)
        } else {
            self.guaranteed_path_regex.is_match(path)
        }
    }
}

fn get_container_type(path: &str) -> &str {
    if path.contains(CONTAINER_TYPE_STR[CONTAINER_TYPE_DOCKER]) {
        return CONTAINER_TYPE_REGEX_STR[CONTAINER_TYPE_DOCKER];
    } else if path.contains(CONTAINER_TYPE_STR[CONTAINER_TYPE_CRI_CONTAINERD]) {
        return CONTAINER_TYPE_REGEX_STR[CONTAINER_TYPE_CRI_CONTAINERD];
    } else if path.contains(CONTAINER_TYPE_STR[CONTAINER_TYPE_CRIO]) {
        return CONTAINER_TYPE_REGEX_STR[CONTAINER_TYPE_CRIO];
    } else if path.contains(CONTAINER_TYPE_STR[CONTAINER_TYPE_K8S_OTHER]) {
        return CONTAINER_TYPE_REGEX_STR[CONTAINER_TYPE_K8S_OTHER];
    }
    return CONTAINER_TYPE_REGEX_STR[CONTAINER_TYPE_UNKNOWN];
}

fn get_cgroup_matcher(path: &str) -> Option<RegMatch> {
    let pod_regex = "[0-9a-f]{8}[-_][0-9a-f]{4}[-_][0-9a-f]{4}[-_][0-9a-f]{4}[-_][0-9a-f]{12}";
    let container_regex = "[0-9a-f]{64}";

    let container_type = get_container_type(path);
    if container_type == CONTAINER_TYPE_REGEX_STR[CONTAINER_TYPE_UNKNOWN] {
        return None;
    }

    let mut matcher = RegMatch {
        burstable_path_regex: Regex::new(&format!(
            "^.*kubepods.slice/kubepods-burstable.slice/kubepods-burstable-pod{}.slice/{}-{}.scope/cgroup.procs$",
            pod_regex, container_type, container_regex
        ))
        .unwrap(),
        besteffort_path_regex: Regex::new(&format!(
            "^.*kubepods.slice/kubepods-besteffort.slice/kubepods-besteffort-pod{}.slice/{}-{}.scope/cgroup.procs$",
            pod_regex, container_type, container_regex
        ))
        .unwrap(),
        guaranteed_path_regex: Regex::new(&format!(
            "^.*kubepods.slice/kubepods-pod{}.slice/{}-{}.scope/cgroup.procs$",
            pod_regex, container_type, container_regex
        ))
        .unwrap(),
    };

    if matcher.is_match(path) {
        return Some(matcher);
    } else {
        matcher.guaranteed_path_regex = Regex::new(&format!(
            "^.*kubepods/pod{}/{}-{}\\.+/cgroup\\.procs$",
            pod_regex, container_type, container_regex
        ))
        .unwrap();
        matcher.besteffort_path_regex = Regex::new(&format!(
            "^.*kubepods/besteffort/pod{}/{}-{}\\.+/cgroup\\.procs$",
            pod_regex, container_type, container_regex
        ))
        .unwrap();
        matcher.burstable_path_regex = Regex::new(&format!(
            "^.*kubepods/burstable/pod{}/{}-{}\\.+/cgroup\\.procs$",
            pod_regex, container_type, container_regex
        ))
        .unwrap();
        if matcher.is_match(path) {
            return Some(matcher);
        } else {
            matcher.guaranteed_path_regex = Regex::new(&format!(
                "^.*docker/{}-{}/cgroup\\.procs$",
                container_type, container_regex
            ))
            .unwrap();
            if matcher.is_match(path) {
                return Some(matcher);
            }
        }
    }
    None
}

fn get_cgroup_path(pid: i32, t: &str) -> Result<String, String> {
    let data = match fs::read_to_string(format!("/proc/{}/cgroup", pid)) {
        Ok(content) => content,
        Err(err) => return Err(err.to_string()),
    };
    let lines: Vec<&str> = data.split('\n').collect();
    for line in lines {
        let parts: Vec<&str> = line.split(':').collect();
        if parts.len() < 3 {
            continue;
        }
        if parts[1] == t {
            return Ok(parts[2].to_string());
        }
    }
    Err(format!("cgroup not found for pid {}", pid))
}

fn get_container_id_by_pid(pid: i32) -> Option<Vec<String>> {
    let mut container_id = String::new();
    let mut pod_id = String::new();
    let path = match get_cgroup_path(pid, "cpu,cpuacct") {
        Ok(path) => path,
        Err(_) => return None,
    };
    let matcher = get_cgroup_matcher(&format!("{}/cgroup.procs", path));
    if matcher.is_none() {
        return None;
    }
    let container_regex = "[0-9a-f]{64}";
    let re_container = Regex::new(container_regex).unwrap();
    container_id = re_container.find(&path)?.as_str().to_string();
    let pod_regex = "[0-9a-f]{8}[-_][0-9a-f]{4}[-_][0-9a-f]{4}[-_][0-9a-f]{4}[-_][0-9a-f]{12}";
    let re_pod = Regex::new(pod_regex).unwrap();
    pod_id = re_pod.find(&path)?.as_str().to_string();
    Some(vec![container_id, pod_id])
}

#[derive(Debug, Default)]
pub struct Pid {
    pub pid: u32,
    pub comm: String,
    pub container_id: String,
    pub podid: String,
}

impl Pid {
    pub fn from_key_and_value(key_bytes: &Vec<u8>, val_bytes: &Vec<u8>) -> Self {
        let (head, body, _tail) = unsafe { val_bytes.align_to::<pid_info>() };
        assert!(head.is_empty(), "Data was not aligned");
        let info = &body[0];

        let pid_arr = [key_bytes[0], key_bytes[1], key_bytes[2],key_bytes[3]];
        let pid = u32::from_ne_bytes(pid_arr);

        // 0-9a-f
        let mut container_id = unsafe {
            String::from_utf8_unchecked(info.container_id.to_vec())
                .trim_matches(char::from(0))
                .to_owned()
        };

        let mut start = 0;
        loop {
            let mut matches = 0;
            for c in container_id[start..].chars() {
                if matches == 12 {
                    break;
                }
                if (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') {
                    matches += 1;
                    continue;
                }
                break;
            }

            if matches != 12 {
                if start >= container_id.len() {
                    break;
                }
            } else {
                container_id = container_id[start..(start + 12)].to_owned();
                break;
            }
            start += 1;
        }

        let mut podid = String::default();
        if let Some(ids) = get_container_id_by_pid(pid as i32) {
            podid = ids[1].clone();
        }

        Pid {
            pid,
            comm: unsafe {
                String::from_utf8_unchecked(info.comm.to_vec())
                    .trim_matches(char::from(0))
                    .to_owned()
            },
            container_id,
            podid,
        }
    }
}

#[derive(Debug, Default)]
pub struct Pids {
    pub pids: HashMap<u32, Pid>,
}

impl Pids {
    pub fn update(&mut self, map: &MapHandle) {
        for key in map.keys() {
            if let Some(val) = map
                .lookup(&key, libbpf_rs::MapFlags::ANY)
                .expect("failed to lookup pid map")
            {
                let pid = Pid::from_key_and_value(&key, &val);

                let pid_arr = [key[0], key[1], key[2], key[3]];
                let pid_num = u32::from_ne_bytes(pid_arr);
                self.pids.insert(pid_num, pid);
            }
        }
    }
}
