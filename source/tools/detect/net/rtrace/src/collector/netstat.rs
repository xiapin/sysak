use anyhow::bail;
use anyhow::Result;
use serde::Deserialize;
use serde::Serialize;
use std::collections::HashMap;
use std::fs::read_to_string;
use std::ops::Sub;
use std::path::Path;
use std::str::FromStr;

#[derive(Default, Debug, Clone, Serialize, Deserialize)]
pub struct Netstat {
    netstat: HashMap<String, isize>,
}

impl FromStr for Netstat {
    type Err = anyhow::Error;
    fn from_str(content: &str) -> Result<Self> {
        let mut netstat = Netstat::default();

        let lines = content.split('\n').collect::<Vec<&str>>();

        for i in 0..lines.len() / 2 {
            let line1 = lines[i * 2];
            let line2 = lines[i * 2 + 1];

            let mut iter1 = line1.split_whitespace();
            let mut iter2 = line2.split_whitespace();

            let prefix;
            if let Some(x) = iter1.next() {
                prefix = x.to_string();
            } else {
                bail!("failed to parse: prefix not found")
            }
            iter2.next();
            loop {
                let k;
                let v: isize;
                if let Some(x) = iter1.next() {
                    k = x;
                } else {
                    break;
                }

                if let Some(x) = iter2.next() {
                    v = x.parse()?;
                } else {
                    bail!("failed to parse: number of item is not match.")
                }

                netstat.insert((prefix.clone(), k.to_string()), v);
            }
        }

        Ok(netstat)
    }
}

impl Netstat {
    pub fn new() -> Result<Netstat> {
        Self::from_file("/proc/net/netstat")
    }

    pub fn from_file<P>(path: P) -> Result<Netstat>
    where
        P: AsRef<Path>,
    {
        let string = read_to_string(path)?;
        Netstat::from_str(&string)
    }

    pub fn insert(&mut self, k: (String, String), v: isize) {
        self.netstat.insert(format!("{}:{}", k.0, k.1), v);
    }
}

impl<'a> Sub<&'a Self> for Netstat {
    type Output = Self;

    fn sub(self, rhs: &'a Self) -> Self::Output {
        let mut result = Netstat::default();

        for (stat_name, value) in self.netstat {
            let rhs_value = rhs.netstat.get(&stat_name).unwrap_or(&0);
            result.netstat.insert(stat_name, value - rhs_value);
        }

        result
    }
}
