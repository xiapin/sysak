use libbpf_cargo::SkeletonBuilder;
use std::env;
use std::path::PathBuf;

fn generate_skeleton(out: &mut PathBuf, name: &str) {
    let c_path = format!("src/bpf/{}.bpf.c", name);
    let rs_name = format!("{}.skel.rs", name);
    out.push(&rs_name);
    SkeletonBuilder::new()
        .source(&c_path)
        .clang_args("-Wno-compare-distinct-pointer-types")
        .build_and_generate(&out)
        .unwrap();

    out.pop();
    println!("cargo:rerun-if-changed={c_path}");
}

fn generate_header(out: &mut PathBuf, name: &str) {
    let header_path = format!("src/bpf/{}.h", name);
    let rs_name = format!("{}.rs", name);

    out.push(&rs_name);
    let bindings = bindgen::Builder::default()
        .header(&header_path)
        .parse_callbacks(Box::new(bindgen::CargoCallbacks))
        .generate()
        .unwrap();
    bindings.write_to_file(&out).unwrap();
    out.pop();

    println!("cargo:rerun-if-changed={header_path}");
}

fn main() {
    let mut out =
        PathBuf::from(env::var_os("OUT_DIR").expect("OUT_DIR must be set in build script"));

    generate_skeleton(&mut out, "ping_sender");
    generate_header(&mut out, "pingtrace");

    generate_skeleton(&mut out, "userslow");
    generate_header(&mut out, "userslow");

    generate_skeleton(&mut out, "queueslow");
    generate_header(&mut out, "queueslow");

    generate_skeleton(&mut out, "drop");
    generate_header(&mut out, "drop");

    generate_skeleton(&mut out, "retran");
    generate_header(&mut out, "retran");

    generate_skeleton(&mut out, "virtio");
    generate_header(&mut out, "virtio");

    generate_skeleton(&mut out, "socket");

    generate_skeleton(&mut out, "tcpping");
    generate_header(&mut out, "tcpping");
}
