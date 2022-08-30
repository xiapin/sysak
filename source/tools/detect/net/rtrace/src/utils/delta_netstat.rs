use anyhow::{bail, Result};
use std::collections::HashMap;
use std::fmt;
use std::fs::{read_to_string, File};
use std::io::{self, BufRead};
use std::ops::Sub;
use std::path::Path;
use std::str::FromStr;

#[derive(Default, Debug, Clone)]
struct Netstat {
    hm: HashMap<(String, String), isize>,
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
    pub fn from_file<P>(path: P) -> Result<Netstat>
    where
        P: AsRef<Path>,
    {
        let string = read_to_string(path)?;
        Netstat::from_str(&string)
    }

    pub fn insert(&mut self, k: (String, String), v: isize) {
        self.hm.insert(k, v);
    }
}

#[derive(Default, Debug, Clone)]
pub struct DeltaNetstat {
    path: String,
    prenetstat: Netstat,
    curnetstat: Netstat,
}

impl DeltaNetstat {
    pub fn new(path: &str) -> Result<DeltaNetstat> {
        let curnetstat = Netstat::from_file(path)?;
        Ok(DeltaNetstat {
            path: path.clone().to_owned(),
            prenetstat: curnetstat.clone(),
            curnetstat,
        })
    }

    pub fn update(&mut self) -> Result<()> {
        std::mem::swap(&mut self.prenetstat, &mut self.curnetstat);
        self.curnetstat = Netstat::from_file(&self.path)?;
        Ok(())
    }
}

impl fmt::Display for DeltaNetstat {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        for (k, v) in &self.curnetstat.hm {
            let pre_v = self.prenetstat.hm.get(k).unwrap();
            if  v - pre_v != 0 {
                write!(f, "{}:{} {} ", k.0, k.1, v - pre_v)?;
            }
        }
        Ok(())
    }
}
