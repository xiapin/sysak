use crate::pid_info;
use libbpf_rs::MapHandle;
use std::collections::HashMap;

#[derive(Debug, Default)]
pub struct Pid {
    pub comm: String,
    pub container_id: String,
}

impl Pid {
    pub fn from_key_and_value(key_bytes: &Vec<u8>, val_bytes: &Vec<u8>) -> Self {
        let (head, body, _tail) = unsafe { val_bytes.align_to::<pid_info>() };
        assert!(head.is_empty(), "Data was not aligned");
        let info = &body[0];

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

        Pid {
            comm: unsafe {
                String::from_utf8_unchecked(info.comm.to_vec())
                    .trim_matches(char::from(0))
                    .to_owned()
            },
            container_id,
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
                let pid = Pid::from_key_and_value(&vec![], &val);

                let pid_arr = [key[0], key[1], key[2], key[3]];
                let pid_num = u32::from_ne_bytes(pid_arr);
                self.pids.insert(pid_num, pid);
            }
        }
    }
}
