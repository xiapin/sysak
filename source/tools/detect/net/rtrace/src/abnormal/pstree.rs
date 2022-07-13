use anyhow::Result;
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
                    self.pids
                        .insert(pid, Pid::from_file(entry_instance.path())?);
                }
                Err(_) => {}
            }
        }

        Ok(())
    }

    /// find pid who own this inode number
    pub fn inum_pid(&self) -> Result<()> {
        Ok(())
    }
}
