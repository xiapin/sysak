use crate::macros::*;
use anyhow::Result;
use procfs::net::DeviceStatus;
use std::collections::HashMap;
use std::fmt;

pub struct DeltaDev {
    predev: HashMap<String, DeviceStatus>,
    curdev: HashMap<String, DeviceStatus>,
}

pub struct DeviceDropStatus {
    pub dev: String,
    pub key: String,
    pub count: isize,
    pub reason: String,
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

    pub fn drop_reason(&self) -> Vec<DeviceDropStatus> {
        let mut ret = vec![];

        let mut stats: Vec<_> = self.curdev.values().collect();
        stats.sort_by_key(|s| &s.name);
        for stat in &stats {
            let subtrahend = self.predev.get(&stat.name).unwrap();

            let mut cnt = same_struct_member_sub!(stat, subtrahend, sent_errs);
            if cnt > 0 {
                ret.push(
                    DeviceDropStatus {
                        dev: stat.name.clone(),
                        key: "sent_errs".to_owned(),
                        count: cnt as isize,
                        reason: "硬件发包出错".into(),
                    }
                );
            }

            cnt = same_struct_member_sub!(stat, subtrahend, sent_drop);
            ret.push(
                DeviceDropStatus {
                    dev: stat.name.clone(),
                    key: "sent_drop".into(),
                    count: cnt as isize,
                    reason: "硬件发包出错".into(),
                }
            );

            cnt = same_struct_member_sub!(stat, subtrahend, sent_fifo);
            ret.push(
                DeviceDropStatus {
                    dev: stat.name.clone(),
                    key: "sent_fifo".into(),
                    count: cnt as isize,
                    reason: "硬件发包出错, fifo缓冲区不足".into(),
                }
            );

            cnt = same_struct_member_sub!(stat, subtrahend, sent_colls);
            ret.push(
                DeviceDropStatus {
                    dev: stat.name.clone(),
                    key: "sent_colls".into(),
                    count: cnt as isize,
                    reason: "硬件发包出错，出现冲突".into(),
                }
            );

            cnt = same_struct_member_sub!(stat, subtrahend, sent_carrier);
            ret.push(
                DeviceDropStatus {
                    dev: stat.name.clone(),
                    key: "sent_carrier".into(),
                    count: cnt as isize,
                    reason: "硬件发包出错，出现冲突".into(),
                }
            );

            cnt = same_struct_member_sub!(stat, subtrahend, recv_errs);
            ret.push(
                DeviceDropStatus {
                    dev: stat.name.clone(),
                    key: "recv_errs".into(),
                    count: cnt as isize,
                    reason: "硬件收包出错，可能是包解析出错".into(),
                }
            );

            cnt = same_struct_member_sub!(stat, subtrahend, recv_drop);
            ret.push(
                DeviceDropStatus {
                    dev: stat.name.clone(),
                    key: "recv_drop".into(),
                    count: cnt as isize,
                    reason: "硬件收包出错".into(),
                }
            );

            cnt = same_struct_member_sub!(stat, subtrahend, recv_fifo);
            ret.push(
                DeviceDropStatus {
                    dev: stat.name.clone(),
                    key: "recv_fifo".into(),
                    count: cnt as isize,
                    reason: "硬件收包出错，fifo缓冲区不足，可能是流量过大".into(),
                }
            );

            cnt = same_struct_member_sub!(stat, subtrahend, recv_frame);
            ret.push(
                DeviceDropStatus {
                    dev: stat.name.clone(),
                    key: "recv_frame".into(),
                    count: cnt as isize,
                    reason: "硬件收包出错".into(),
                }
            );
        }


        ret
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
