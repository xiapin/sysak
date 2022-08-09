static ALIOS_URL: &'static str =
    "http://yum.tbsite.net/taobao/7/x86_64/current/kernel-debuginfo/kernel-debuginfo-{}.rpm";
static CENTOS_URL: &'static str = "http://debuginfo.centos.org/7/x86_64/kernel-debuginfo-{}.rpm";
static ALINUX_URL: &'static str =
    "https://mirrors.aliyun.com/alinux/2.1903/plus/x86_64/debug/kernel-debuginfo-{}.rpm";
static AN7_URL: &'static str =
    "https://mirrors.aliyun.com/anolis/7.7/os/x86_64/debug/Packages/kernel-debuginfo-{}.rpm";
static AN7_PLUS: &'static str =
    "https://mirrors.aliyun.com/anolis/7.7/Plus/x86_64/debug/Packages/kernel-debuginfo-{}.rpm";
static AN8_URL: &'static str =
    "https://mirrors.aliyun.com/anolis/8/BaseOS/x86_64/debug/Packages/kernel-debuginfo-{}.rpm";
static AN82_URL: &'static str =
    "https://mirrors.aliyun.com/anolis/8.2/BaseOS/x86_64/debug/Packages/kernel-debuginfo-{}.rpm";
static AN84_URL: &'static str =
    "https://mirrors.aliyun.com/anolis/8.4/BaseOS/x86_64/debug/Packages/kernel-debuginfo-{}.rpm";
static AN84_PLUS: &'static str =
    "https://mirrors.aliyun.com/anolis/8.4/Plus/x86_64/debug/Packages/kernel-debuginfo-{}.rpm";

use anyhow::{bail, Result};
use crossbeam_channel;
use std::thread;
use std::time;
use structopt::StructOpt;

#[derive(Debug, StructOpt)]
pub struct BtfgenCommand {
    #[structopt(long, help = "Kernel version of BTF file to be generated")]
    version: Option<String>,
}

fn parse_url(version: &String) -> Result<(String, String)> {
    if version.contains("al7") {
        let url = format!(
            "https://mirrors.aliyun.com/alinux/2.1903/plus/x86_64/debug/kernel-debuginfo-{}.rpm",
            version
        );
        let package_name = format!("kernel-debuginfo-{}.rpm", version);
        return Ok((url, package_name));
    }

    bail!("failed to get rpm url, kernel version {}", version)
}

pub fn build_btfgen(opts: &BtfgenCommand) -> Result<()> {
    let mut version = "".to_owned();
    if let Some(x) = &opts.version {
        version = x.clone();
    }

    let (url, package_name) = parse_url(&version)?;
    let btf_path = format!("vmlinux-{}", version);

    println!("wget {}", url);
    // let output = std::process::Command::new("wget").arg(url).output()?;

    // let rpmcmd = format!("rpm2cpio {} | cpio -idcv", package_name);
    // println!("{}", rpmcmd);
    // let output = std::process::Command::new(rpmcmd).output()?;

    let vmlinux_path = format!("./usr/lib/debug/lib/modules/{}/vmlinux", version);
    let pahole_cmd = format!(
        "pahole -J --btf_encode_detached={} --kabi_prefix=__UNIQUE_ID_rh_kabi_hide {}",
        btf_path, vmlinux_path
    );
    println!("{}", pahole_cmd);
    let output = std::process::Command::new(pahole_cmd).output()?;
    Ok(())
}
