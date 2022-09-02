use std::env;
use std::fmt::format;
use std::fs::create_dir_all;
use std::path::{Path, PathBuf};

use libbpf_cargo::SkeletonBuilder;
use bpfskel::BpfSkel;


const APPS: &'static [&'static str] = &[ "abnormal", "drop", "latency"];
const COMMONHDR: &str = "common.h";


fn compile_hdr(hdrfile: &str, bindingfile: &str) {

    let bindings = bindgen::Builder::default()
        // The input header we would like to generate
        // bindings for.
        .header(hdrfile)
        // Tell cargo to invalidate the built crate whenever any of the
        // included header files changed.
        .parse_callbacks(Box::new(bindgen::CargoCallbacks))
        // Finish the builder and generate the bindings.
        .generate()
        // Unwrap the Result and panic on failure.
        .expect("Unable to generate bindings");

    bindings
        .write_to_file(bindingfile)
        .expect("Couldn't write bindings!");
}

fn compile_bpf(bpfpath: &str, objpath: &str, skelpath: &str) {
    match SkeletonBuilder::new()
        .source(bpfpath)
        .obj(objpath)
        .build_and_generate(&skelpath)
    {
        Ok(()) => {}
        Err(e) => {
            println!("{}", e);
            panic!()
        }
    }
}

fn compile_app(app: &str) {
    let bpfdir = format!("src/{}/bpf/", app);
    let outputdir = format!("{}/.output", bpfdir);

    create_dir_all(&outputdir).unwrap();

    // compile bpf code
    let bpffile = format!("{}/{}.bpf.c", bpfdir, app);
    let objfile = format!("{}/{}.bpf.o", outputdir, app);
    let skelfile = format!("{}/{}.skel.rs", outputdir, app);
    let skel = format!("{}/skel.rs", outputdir);
    compile_bpf(&bpffile, &objfile, &skelfile);
    BpfSkel::new().obj(&objfile).generate(&skel).unwrap();

    // compile hdr
    let hdrfile = format!("{}/{}.h", bpfdir, app);
    let bindingfile = format!("{}/bindings.rs", outputdir);
    compile_hdr(&hdrfile, &bindingfile);
}

fn main() {
    // run by default
    let build_enabled = env::var("BUILD_ENABLED").map(|v| v == "1").unwrap_or(true);

    if !build_enabled {
        return;
    }

    println!("cargo:rerun-if-changed=./src/drop/bpf/drop.h");

    println!("cargo:rerun-if-changed={}", COMMONHDR);
    for app in APPS {
        let bpffile = format!("./src/{}/bpf/{}.bpf.c", *app, *app);
        let hdrfile = format!("./src/{}/bpf/{}.h", *app, *app);
        println!("cargo:rerun-if-changed={}", bpffile);
        println!("cargo:rerun-if-changed={}", hdrfile);
    }

    for app in APPS {
        compile_app(*app);
    }

    create_dir_all("src/bindings").unwrap();
    let commonbinding = format!("src/bindings/commonbinding.rs");
    compile_hdr(COMMONHDR, &commonbinding);

}
