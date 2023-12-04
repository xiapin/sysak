use anyhow::Result;
use procfs::net::DeviceStatus;
use serde::Deserialize;
use serde::Serialize;
use std::collections::HashMap;
use std::ops::Sub;

#[derive(Default, Debug, Clone, Deserialize, Serialize)]
pub struct NetDev {
    dev: HashMap<String, HashMap<String, isize>>,
}

fn device_status_to_hashmap(ds: &DeviceStatus) -> HashMap<String, isize> {
    let mut hm = HashMap::default();

    hm.insert("recv_bytes".to_owned(), ds.recv_bytes as isize);
    hm.insert("recv_packets".to_owned(), ds.recv_packets as isize);
    hm.insert("recv_errs".to_owned(), ds.recv_errs as isize);
    hm.insert("recv_drop".to_owned(), ds.recv_drop as isize);
    hm.insert("recv_fifo".to_owned(), ds.recv_fifo as isize);
    hm.insert("recv_frame".to_owned(), ds.recv_frame as isize);
    hm.insert("recv_compressed".to_owned(), ds.recv_compressed as isize);
    hm.insert("recv_multicast".to_owned(), ds.recv_multicast as isize);
    hm.insert("sent_bytes".to_owned(), ds.sent_bytes as isize);
    hm.insert("sent_packets".to_owned(), ds.sent_packets as isize);
    hm.insert("sent_errs".to_owned(), ds.sent_errs as isize);
    hm.insert("sent_drop".to_owned(), ds.sent_drop as isize);
    hm.insert("sent_fifo".to_owned(), ds.sent_fifo as isize);
    hm.insert("sent_colls".to_owned(), ds.sent_colls as isize);
    hm.insert("sent_carrier".to_owned(), ds.sent_carrier as isize);
    hm.insert("sent_compressed".to_owned(), ds.sent_compressed as isize);

    hm
}

impl NetDev {
    pub fn new() -> Result<Self> {
        let devs = procfs::net::dev_status()?;
        let mut hm = HashMap::default();
        for (name, dev) in devs {
            hm.insert(name, device_status_to_hashmap(&dev));
        }

        Ok(Self { dev: hm })
    }
}

impl<'a> Sub<&'a Self> for NetDev {
    type Output = Self;

    fn sub(self, rhs: &'a Self) -> Self::Output {
        let mut result = NetDev::default();

        for (dev_name, metrics) in self.dev {
            let rhs_metrics = rhs.dev.get(&dev_name);

            let mut result_metrics = HashMap::default();
            for (metric_name, value) in metrics {
                let rhs_value = rhs_metrics
                    .and_then(|rhs_metrics| rhs_metrics.get(&metric_name))
                    .unwrap_or(&0);

                // Subtract rhs_value from value
                result_metrics.insert(metric_name.clone(), value - rhs_value);
            }

            result.dev.insert(dev_name, result_metrics);
        }

        result
    }
}
