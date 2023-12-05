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
pub struct Snmp {
    snmp: HashMap<String, isize>,
}

impl FromStr for Snmp {
    type Err = anyhow::Error;
    fn from_str(content: &str) -> Result<Self> {
        let mut snmp = Snmp::default();

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

                snmp.insert((prefix.clone(), k.to_string()), v);
            }
        }

        Ok(snmp)
    }
}

impl Snmp {
    pub fn new() -> Result<Snmp> {
        Self::from_file("/proc/net/snmp")
    }

    pub fn from_file<P>(path: P) -> Result<Snmp>
    where
        P: AsRef<Path>,
    {
        let string = read_to_string(path)?;
        Snmp::from_str(&string)
    }

    pub fn insert(&mut self, k: (String, String), v: isize) {
        self.snmp.insert(format!("{}:{}", k.0, k.1), v);
    }
}

impl<'a> Sub<&'a Self> for Snmp {
    type Output = Self;

    fn sub(self, rhs: &'a Self) -> Self::Output {
        let mut result = Snmp::default();

        for (stat_name, value) in self.snmp {
            let rhs_value = rhs.snmp.get(&stat_name).unwrap_or(&0);
            result.snmp.insert(stat_name, value - rhs_value);
        }

        result
    }
}
