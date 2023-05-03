use std::{env, path::PathBuf};
use libbpf_cargo::SkeletonBuilder;
const SRC: &str = "src/bpf/drop.bpf.c";
const HDR: &str = "src/bpf/drop.h";

fn main() {
    let mut out = PathBuf::from(env::var_os("OUT_DIR").expect("OUT_DIR must be set in build script"));
    out.push("drop.skel.rs");
    SkeletonBuilder::new()
        .source(SRC)
        .build_and_generate(&out)
        .unwrap();
    
    // let bindings = bindgen::Builder::default()
    //     .header(HDR)
    //     .parse_callbacks(Box::new(bindgen::CargoCallbacks))
    //     .generate()
    //     .expect("Unable to generate bindings for drop crate");

    // out.pop();
    // out.push("bindings.rs");
    // bindings
    //     .write_to_file(&out)
    //     .expect("Couldn't write bindings for drop crate!");
    
    println!("cargo:rerun-if-changed={}", SRC);
    println!("cargo:rerun-if-changed={}", HDR);
}
