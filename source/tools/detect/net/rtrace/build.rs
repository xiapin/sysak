use std::env;
use std::fs::create_dir_all;
use std::path::Path;

use libbpf_cargo::SkeletonBuilder;

const LATENCY_TCP_SRC: &str = "./src/latencylegacy/bpf/tcp.bpf.c";
const LATENCY_ICMP_SRC: &str = "./src/latencylegacy/bpf/icmp.bpf.c";
const LATENCY_HDR: &str = "./src/latencylegacy/bpf/rtrace.h";

const DROP_BPF_SRC: &str = "./src/drop/bpf/drop.bpf.c";
const DROP_HDR: &str = "./src/drop/bpf/drop.h";

const ABNORMAL_TCP_BPF_SRC: &str = "./src/abnormal/bpf/tcp.bpf.c";
const ABNORMAL_HDR: &str = "./src/abnormal/bpf/abnormal.h";

const SLI_BPF_SRC: &str = "./src/sli/bpf/sli.bpf.c";
const SLI_HDR: &str = "./src/sli/bpf/sli.h";

fn compile_sli_ebpf() {
    create_dir_all("./src/sli/bpf/.output").unwrap();

    let sli_skel = Path::new("./src/sli/bpf/.output/sli.skel.rs");
    match SkeletonBuilder::new()
        .source(SLI_BPF_SRC)
        .build_and_generate(&sli_skel)
    {
        Ok(()) => {}
        Err(e) => {
            println!("{}", e);
            panic!()
        }
    }

    let bindings = bindgen::Builder::default()
        // The input header we would like to generate
        // bindings for.
        .header(SLI_HDR)
        // Tell cargo to invalidate the built crate whenever any of the
        // included header files changed.
        .parse_callbacks(Box::new(bindgen::CargoCallbacks))
        // Finish the builder and generate the bindings.
        .generate()
        // Unwrap the Result and panic on failure.
        .expect("Unable to generate bindings");
    let bind = Path::new("./src/sli/bpf/.output/bindings.rs");
    bindings
        .write_to_file(bind)
        .expect("Couldn't write bindings!");
}

fn compile_abnormal_ebpf() {
    create_dir_all("./src/abnormal/bpf/.output").unwrap();

    let tcp_skel = Path::new("./src/abnormal/bpf/.output/tcp.skel.rs");
    match SkeletonBuilder::new()
        .source(ABNORMAL_TCP_BPF_SRC)
        .build_and_generate(&tcp_skel)
    {
        Ok(()) => {}
        Err(e) => {
            println!("{}", e);
            panic!()
        }
    }

    let bindings = bindgen::Builder::default()
        // The input header we would like to generate
        // bindings for.
        .header(ABNORMAL_HDR)
        // Tell cargo to invalidate the built crate whenever any of the
        // included header files changed.
        .parse_callbacks(Box::new(bindgen::CargoCallbacks))
        // Finish the builder and generate the bindings.
        .generate()
        // Unwrap the Result and panic on failure.
        .expect("Unable to generate bindings");
    let bind = Path::new("./src/abnormal/bpf/.output/bindings.rs");
    bindings
        .write_to_file(bind)
        .expect("Couldn't write bindings!");
}

fn compile_drop_ebpf() {
    create_dir_all("./src/drop/bpf/.output").unwrap();

    let drop_skel = Path::new("./src/drop/bpf/.output/drop.skel.rs");
    match SkeletonBuilder::new()
        .source(DROP_BPF_SRC)
        .build_and_generate(&drop_skel)
    {
        Ok(()) => {}
        Err(e) => {
            println!("{}", e);
            panic!()
        }
    }

    let bindings = bindgen::Builder::default()
        // The input header we would like to generate
        // bindings for.
        .header(DROP_HDR)
        // Tell cargo to invalidate the built crate whenever any of the
        // included header files changed.
        .parse_callbacks(Box::new(bindgen::CargoCallbacks))
        // Finish the builder and generate the bindings.
        .generate()
        // Unwrap the Result and panic on failure.
        .expect("Unable to generate bindings");
    let bind = Path::new("./src/drop/bpf/.output/bindings.rs");
    bindings
        .write_to_file(bind)
        .expect("Couldn't write bindings!");
}

fn compile_latency_legacy_ebpf() {
    create_dir_all("./src/latencylegacy/bpf/.output").unwrap();
    let tcp_skel = Path::new("./src/latencylegacy/bpf/.output/tcp.skel.rs");
    match SkeletonBuilder::new()
        .source(LATENCY_TCP_SRC)
        .build_and_generate(&tcp_skel)
    {
        Ok(()) => {}
        Err(e) => {
            println!("{}", e);
            panic!()
        }
    }

    let icmp_skel = Path::new("./src/latencylegacy/bpf/.output/icmp.skel.rs");
    match SkeletonBuilder::new()
        .source(LATENCY_ICMP_SRC)
        .build_and_generate(&icmp_skel)
    {
        Ok(()) => {}
        Err(e) => {
            println!("{}", e);
            panic!()
        }
    }

    let bindings = bindgen::Builder::default()
        // The input header we would like to generate
        // bindings for.
        .header(LATENCY_HDR)
        // Tell cargo to invalidate the built crate whenever any of the
        // included header files changed.
        .parse_callbacks(Box::new(bindgen::CargoCallbacks))
        // Finish the builder and generate the bindings.
        .generate()
        // Unwrap the Result and panic on failure.
        .expect("Unable to generate bindings");
    let bind = Path::new("./src/latencylegacy/bpf/.output/bindings.rs");
    bindings
        .write_to_file(bind)
        .expect("Couldn't write bindings!");
}

fn main() {
    // run by default
    let build_enabled = env::var("BUILD_ENABLED").map(|v| v == "1").unwrap_or(true);

    if !build_enabled {
        return;
    }

    // It's unfortunate we cannot use `OUT_DIR` to store the generated skeleton.
    // Reasons are because the generated skeleton contains compiler attributes
    // that cannot be `include!()`ed via macro. And we cannot use the `#[path = "..."]`
    // trick either because you cannot yet `concat!(env!("OUT_DIR"), "/skel.rs")` inside
    // the path attribute either (see https://github.com/rust-lang/rust/pull/83366).
    //
    // However, there is hope! When the above feature stabilizes we can clean this
    // all up.
    println!("cargo:rerun-if-changed={}", DROP_BPF_SRC);
    println!("cargo:rerun-if-changed={}", DROP_HDR);
    println!("cargo:rerun-if-changed={}", LATENCY_TCP_SRC);
    println!("cargo:rerun-if-changed={}", LATENCY_ICMP_SRC);
    println!("cargo:rerun-if-changed={}", LATENCY_HDR);

    println!("cargo:rerun-if-changed={}", ABNORMAL_TCP_BPF_SRC);
    println!("cargo:rerun-if-changed={}", ABNORMAL_HDR);

    println!("cargo:rerun-if-changed={}", SLI_BPF_SRC);
    println!("cargo:rerun-if-changed={}", SLI_HDR);

    compile_latency_legacy_ebpf();
    compile_drop_ebpf();
    compile_abnormal_ebpf();
    compile_sli_ebpf();
}
