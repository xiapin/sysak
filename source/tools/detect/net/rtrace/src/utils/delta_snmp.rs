use anyhow::Result;
use eutils_rs::proc::{Netstat, Snmp};
use std::fmt;

#[derive(Default, Debug, Clone)]
pub struct DeltaSnmp {
    path: String,
    presnmp: Snmp,
    cursnmp: Snmp,
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
}

impl fmt::Display for DeltaSnmp {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        // todo
        Ok(())
    }
}
