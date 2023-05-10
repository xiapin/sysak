use anyhow::{bail, Result};
use bpfskel::BpfSkel;
use libbpf_cargo::SkeletonBuilder;
use std::path::{Path, PathBuf};
use structopt::StructOpt;
use tempfile::tempdir;

#[derive(Debug, StructOpt)]
pub struct BpfSkelCli {
    #[structopt(short, long, help = "path of bpf source, such as *.bpf.c")]
    bpfsrc: String,
    #[structopt(short, long, default_value = "./skel.rs", help = "skel.rs location")]
    skel: String,
}

fn main() -> Result<()> {
    let opts = BpfSkelCli::from_args();

    let path = Path::new(&opts.bpfsrc);
    if !path.exists() {
        bail!("{} file not exist", opts.bpfsrc)
    }

    let filename = path.file_name().unwrap().to_str().unwrap();
    if !opts.bpfsrc.ends_with(".bpf.c") {
        bail!("file not end with .bpf.c")
    }
    let basename = filename[0..filename.len() - ".bpf.c".len()].to_owned();

    let dir = tempdir()?;
    let obj_path = dir.path().join(format!("{}.bpf.o", basename));
    let skel_path = dir.path().join(format!("{}.skel.rs", basename));

    match SkeletonBuilder::new()
        .source(&opts.bpfsrc)
        .obj(&obj_path)
        .build_and_generate(&skel_path)
    {
        Ok(()) => {}
        Err(e) => {
            println!("{}", e);
            panic!()
        }
    }

    BpfSkel::new().obj(&obj_path).generate(&opts.skel).unwrap();
    Ok(())
}
