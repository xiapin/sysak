use serde::Deserialize;
use serde::Serialize;
use std::fmt;
/// process information
#[derive(Clone, Serialize, Deserialize, Debug, Default)]
pub struct Process {
    pub pid: u32,
    pub comm: String,
}

impl Process {
    pub fn new(pid: u32, mut comm: Vec<u8>) -> Self {
        let len = comm.iter().position(|&x| x == 0).unwrap_or(comm.len());
        comm.truncate(len);
        Process {
            pid,
            comm: unsafe {
                String::from_utf8_unchecked(comm)
                    .trim_matches(char::from(0))
                    .to_owned()
            },
        }
    }
}

impl fmt::Display for Process {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}/{}", self.pid, self.comm)
    }
}

/// kernel schedule switch information
#[derive(Clone, Serialize, Deserialize, Debug)]
pub struct Sched {
    pub prev: Process,
    pub next: Process,
}

impl Sched {
    pub fn new(prev: Process, next: Process) -> Self {
        Sched { prev, next }
    }
}

impl fmt::Display for Sched {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{} -> {}", self.prev, self.next)
    }
}
