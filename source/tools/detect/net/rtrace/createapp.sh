
#!/bin/bash
set -e

appname=$1
appname_up=${appname^^}

# create dir
mkdir src/$appname
mkdir src/$appname/bpf

# enter target dir
cd src/$appname/bpf
# link files
ln -s ../../../vmlinux_515.h vmlinux.h
ln -s ../../../bpf_core.h bpf_core.h
ln -s ../../../common.h common.h


touch $appname.bpf.c 
cat > $appname.bpf.c << EOF
#define BPF_NO_GLOBAL_DATA
#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_endian.h>

#include "common.h"
#include "bpf_core.h"
#include "$appname.h"


char _license[] SEC("license") = "GPL";
EOF

touch $appname.h
cat > $appname.h << EOF
#ifndef __${appname_up}_H
#define __${appname_up}_H
#include "common.h"

#endif
EOF


cd ..
touch mod.rs
cat > mod.rs << EOF
mod $appname;
EOF
touch $appname.rs
cat > $appname.rs << EOF
use crate::utils::macros::*;
ebpf_common_use!($appname);

#[derive(Debug, StructOpt)]
pub struct ${appname^}Command {
    #[structopt(long, help = "Process identifier of container")]
    pid: Option<usize>,
    #[structopt(long, help = "Local network address of traced sock")]
    src: Option<String>,
    #[structopt(long, help = "Remote network address of traced sock")]
    dst: Option<String>,
}

pub fn build_${appname}(opts: &${appname^}Command, debug: bool, btf: &Option<String>) -> Result<()> {
    bail!("not support yet")
}

EOF