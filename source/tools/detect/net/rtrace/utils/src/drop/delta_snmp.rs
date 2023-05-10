use anyhow::{bail, Result};
use std::collections::HashMap;
use std::fmt;
use std::fs::{read_to_string, File};
use std::io::{self, BufRead};
use std::ops::Sub;
use std::path::Path;
use std::str::FromStr;

use serde::{Deserialize, Serialize};

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

#[derive(Default, Debug, Clone)]
pub struct DeltaSnmp {
    path: String,
    presnmp: Snmp,
    cursnmp: Snmp,
}

pub struct SnmpDropStatus {
    pub key: String,
    pub count: isize,
    pub reason: String,
}

impl DeltaSnmp {
    pub fn new(path: &str) -> Result<DeltaSnmp> {
        let cursnmp = Snmp::from_file(path)?;
        Ok(DeltaSnmp {
            path: path.clone().to_owned(),
            presnmp: cursnmp.clone(),
            cursnmp,
        })
    }

    pub fn update(&mut self) -> Result<()> {
        std::mem::swap(&mut self.presnmp, &mut self.cursnmp);
        self.cursnmp = Snmp::from_file(&self.path)?;
        Ok(())
    }

    fn delta(&self, key: &(String, String)) -> Option<isize> {
        // if let Some(x) = self.cursnmp.hm.get(&key) {
        //     if let Some(y) = self.presnmp.hm.get(&key) {
        //         return Some(*x - *y);
        //     }
        // }
        None
    }

    pub fn drop_reason(&self) -> Vec<SnmpDropStatus> {
        let mut ret = vec![];

        let mut key = ("Tcp:".into(), "InCsumErrors".into());

        if let Some(x) = self.delta(&key) {
            ret.push(SnmpDropStatus {
                key: key.1.clone(),
                count: x,
                reason: "报文的checksum值不对，报文数据可能被硬件篡改".into(),
            });
        }

        ret
    }
}

impl fmt::Display for DeltaSnmp {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        // for (k, v) in &self.cursnmp.hm {
        //     let pre_v = self.presnmp.hm.get(k).unwrap();
        //     if v - pre_v != 0 {
        //         write!(f, "{}:{} {} ", k.0, k.1, v - pre_v)?;
        //     }
        // }
        Ok(())
    }
}
