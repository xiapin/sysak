use crate::utils::macros::*;
use anyhow::Result;
use procfs::net::DeviceStatus;
use std::collections::HashMap;
use std::fmt;

pub struct DeltaDev {
    predev: HashMap<String, DeviceStatus>,
    curdev: HashMap<String, DeviceStatus>,
}

impl DeltaDev {
    pub fn new() -> Result<DeltaDev> {
        let curdev = procfs::net::dev_status()?;
        Ok(DeltaDev {
            predev: curdev.clone(),
            curdev,
        })
    }

    pub fn update(&mut self) -> Result<()> {
        std::mem::swap(&mut self.predev, &mut self.curdev);
        self.curdev = procfs::net::dev_status()?;
        Ok(())
    }
}

impl fmt::Display for DeltaDev {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let mut stats: Vec<_> = self.curdev.values().collect();

        stats.sort_by_key(|s| &s.name);
        writeln!(
            f,
            "{:<10} {:<10} {:<10} {:<10} {:<10} {:<10}",
            "Interface", "SendErrs", "SendDrop", "SendFifo", "SendColls", "SendCarrier"
        )?;

        for stat in &stats {
            let subtrahend = self.predev.get(&stat.name).unwrap();
            writeln!(
                f,
                "{:<10} {:<10} {:<10} {:<10} {:<10} {:<10}",
                stat.name,
                same_struct_member_sub!(stat, subtrahend, sent_errs),
                same_struct_member_sub!(stat, subtrahend, sent_drop),
                same_struct_member_sub!(stat, subtrahend, sent_fifo),
                same_struct_member_sub!(stat, subtrahend, sent_colls),
                same_struct_member_sub!(stat, subtrahend, sent_carrier),
            )?;
        }

        writeln!(
            f,
            "{:<10} {:<10} {:<10} {:<10} {:<10}",
            "Interface", "RecvErrs", "RecvDrop", "RecvFifo", "RecvFrameErr"
        )?;

        for stat in &stats {
            let subtrahend = self.predev.get(&stat.name).unwrap();
            writeln!(
                f,
                "{:<10} {:<10} {:<10} {:<10} {:<10}",
                stat.name,
                same_struct_member_sub!(stat, subtrahend, recv_errs),
                same_struct_member_sub!(stat, subtrahend, recv_drop),
                same_struct_member_sub!(stat, subtrahend, recv_fifo),
                same_struct_member_sub!(stat, subtrahend, recv_frame),
            )?;
        }

        write!(f, "")
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn test_delta_dev() {
        let delta = DeltaDev::new();
        assert!(delta.is_ok());
    }

    #[test]
    fn test_delta_dev_update_display() {
        let mut delta = DeltaDev::new().unwrap();
        std::thread::sleep(std::time::Duration::from_millis(100));
        delta.update().unwrap();
        println!("{}", delta);
    }
}
