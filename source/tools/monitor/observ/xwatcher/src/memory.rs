use procfs::{process::Stat, Meminfo};

#[derive(Default, Debug)]
pub struct Memory {
    pub rss: u64,
    pub vms: u64,
    pub pct: f32, // rss / total
}

impl Memory {
    pub fn new(stat: &Stat, minfo: &Meminfo) -> Self {
        let rss = stat.rss;
        let vms = stat.vsize;

        Memory {
            rss,
            vms,
            pct: (rss * 100) as f32 / minfo.mem_total as f32,
        }
    }
}

impl ToString for Memory {
    fn to_string(&self) -> String {
        format!(
            "memRss={},memVms={},memPct={:.2}",
            self.rss, self.vms, self.pct
        )
    }
}
