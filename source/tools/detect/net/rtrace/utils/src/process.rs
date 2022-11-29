use std::fs::File;

use anyhow::Result;
use procfs::process;

pub struct Stat {
    pid: u32,
    stat: process::Stat,
    now_stat: process::Stat,
}

fn get_process_stat(pid: u32) -> Result<process::Stat> {
    Ok(process::Stat::from_reader(File::open(format!("/proc/{}/stat", pid))?)?)
}

impl Stat {
    pub fn new(pid: u32) -> Result<Self> {
        let stat = get_process_stat(pid)?;
        Ok(Stat {
            pid,
            stat: stat.clone(),
            now_stat: stat,
        })
    }

    pub fn update(&mut self) -> Result<()>{
        std::mem::swap(&mut self.stat, &mut self.now_stat);
        self.now_stat = get_process_stat(self.pid)?;
        Ok(())
    }

    pub fn ucpu_usage(&self, delta: usize) {
        // self.now_stat.utime
    }

    pub fn scpu_usage(&self) {

    }

    pub fn mem_usage(&self) {

    }
}
