use anyhow::Result;
use eutils_rs::proc::Netstat;
use std::fmt;

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
        // todo
        Ok(())
    }
}
