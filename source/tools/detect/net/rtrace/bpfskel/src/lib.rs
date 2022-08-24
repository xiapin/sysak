use anyhow::{bail, Result};
use std::path::Path;
use std::path::PathBuf;
mod gen;
mod btf;

pub struct BpfSkel {
    obj: Option<PathBuf>,
    rustfmt: PathBuf,
}

impl BpfSkel {
    pub fn new() -> Self {
        BpfSkel {
            obj: None,
            rustfmt: "rustfmt".into(),
        }
    }

    pub fn obj<P: AsRef<Path>>(&mut self, obj: P) -> &mut BpfSkel {
        self.obj = Some(obj.as_ref().to_path_buf());
        self
    }

    pub fn generate<P: AsRef<Path>>(&mut self, output: P) -> Result<()> {
        if let Some(obj) = &self.obj {
            let out = output.as_ref().to_path_buf();
            gen::gen_single(obj, &out, &self.rustfmt)?;
            return Ok(());
        }
        bail!("no obj file")
    }
}
