use crate::utils::{cached_clock_tick, METRICS_PERIOD};
use procfs::process::Stat;

#[derive(Default, Debug)]
pub struct Cpu {
    pub usr: f32,
    pub sys: f32,
    pub tot: f32,
}

impl Cpu {
    pub fn new(last: &Stat, now: &Stat) -> Self {
        let udelta = (now.utime - last.utime) * 100;
        let sdelta = (now.stime - last.stime) * 100;

        let total = ((METRICS_PERIOD as i64) * cached_clock_tick()) as f32;

        Cpu {
            usr: udelta as f32 / total,
            sys: sdelta as f32 / total,
            tot: (udelta + sdelta) as f32 / total,
        }
    }
}

impl ToString for Cpu {
    fn to_string(&self) -> String {
        format!(
            "cpuUsr={:.2},cpuSys={:.2},cpuTot={:.2}",
            self.usr, self.sys, self.tot
        )
    }
}
