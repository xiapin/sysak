use anyhow::{bail, Result};
use eutils_rs::proc::FdType;
use eutils_rs::proc::Pid;
use std::collections::HashMap;
use std::fs;

pub struct Pstree {
    pids: HashMap<i32, Pid>,
    // map of socket inode number and pid
    inum_map: HashMap<u32, i32>,
}

impl Pstree {
    pub fn new() -> Pstree {
        Pstree {
            pids: HashMap::default(),
            inum_map: HashMap::default(),
        }
    }

    pub fn update(&mut self) -> Result<()> {
        for entry in fs::read_dir("/proc")? {
            let entry_instance = entry?;
            match entry_instance.file_name().to_string_lossy().parse::<i32>() {
                Ok(pid) => {
                    self.pids.insert(pid, Pid::from_file(entry_instance.path()));
                }
                Err(_) => {}
            }
        }

        for (i, j) in &self.pids {
            for fd in &j.fds {
                match fd.fdtype() {
                    FdType::SocketFd(inum) => {
                        self.inum_map.entry(inum).or_insert(*i);
                    }
                    _ => {}
                }
            }
        }

        Ok(())
    }

    /// find pid who own this inode number
    pub fn inum_pid(&self, inum: u32) -> Result<i32> {
        if let Some(pid) = self.inum_map.get(&inum) {
            return Ok(*pid);
        }
        bail!("failed to find pid of {}", inum)
    }

    pub fn pid_comm(&self, pid: i32) -> Result<String> {
        // /proc/<PID>/comm
        let path = format!("/proc/{}/comm", pid);
        match fs::read_to_string(&path) {
            Ok(comm) => {
                return Ok(String::from(comm.trim()));
            }
            Err(e) => {
                bail!("failed to open {}, error: {}", path, e)
            }
        }
    }
}
