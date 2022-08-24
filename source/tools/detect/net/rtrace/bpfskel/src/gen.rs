use crate::btf;
use memmap2::Mmap;
use std::collections::BTreeMap;
use std::convert::TryInto;
use std::ffi::{c_void, CStr, CString};
use std::fmt::Write as fmt_write;
use std::fs::File;
use std::io::Write;
use std::os::raw::c_ulong;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::ptr;

use anyhow::{bail, ensure, Context, Result};

macro_rules! gen_bpf_object_iter {
    ($name:ident, $iter_ty:ty, $next_fn:expr) => {
        struct $name {
            obj: *mut libbpf_sys::bpf_object,
            last: *mut $iter_ty,
        }

        impl $name {
            fn new(obj: *mut libbpf_sys::bpf_object) -> $name {
                $name {
                    obj,
                    last: ptr::null_mut(),
                }
            }
        }

        impl Iterator for $name {
            type Item = *mut $iter_ty;

            fn next(&mut self) -> Option<Self::Item> {
                self.last = unsafe { $next_fn(self.obj, self.last) };

                if self.last.is_null() {
                    None
                } else {
                    Some(self.last)
                }
            }
        }
    };
}

gen_bpf_object_iter!(
    MapIter,
    libbpf_sys::bpf_map,
    libbpf_sys::bpf_object__next_map
);
gen_bpf_object_iter!(
    ProgIter,
    libbpf_sys::bpf_program,
    libbpf_sys::bpf_object__next_program
);

/// Run `rustfmt` over `s` and return result
fn rustfmt(s: &str, rustfmt_path: &Path) -> Result<String> {
    let mut cmd = Command::new(rustfmt_path)
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .spawn()
        .context("Failed to spawn rustfmt")?;

    // Send input in via stdin
    write!(cmd.stdin.take().unwrap(), "{}", s)?;

    // Extract output
    let output = cmd
        .wait_with_output()
        .context("Failed to execute rustfmt")?;
    ensure!(
        output.status.success(),
        "Failed to rustfmt: {}",
        output.status
    );

    Ok(String::from_utf8(output.stdout)?)
}

fn capitalize_first_letter(s: &str) -> String {
    if s.is_empty() {
        return "".to_string();
    }

    s.split('_').fold(String::new(), |mut acc, ts| {
        acc += &ts.chars().next().unwrap().to_uppercase().to_string();
        if ts.len() > 1 {
            acc += &ts[1..];
        }
        acc
    })
}

fn get_raw_map_name(map: *const libbpf_sys::bpf_map) -> Result<String> {
    let name_ptr = unsafe { libbpf_sys::bpf_map__name(map) };
    if name_ptr.is_null() {
        bail!("Map name unknown");
    }

    Ok(unsafe { CStr::from_ptr(name_ptr) }.to_str()?.to_string())
}

fn canonicalize_internal_map_name(s: &str) -> Option<String> {
    if s.ends_with(".data") {
        Some("data".to_string())
    } else if s.ends_with(".rodata") {
        Some("rodata".to_string())
    } else if s.ends_with(".bss") {
        Some("bss".to_string())
    } else if s.ends_with(".kconfig") {
        Some("kconfig".to_string())
    } else {
        eprintln!("Warning: unrecognized map: {}", s);
        None
    }
}

/// Same as `get_raw_map_name` except the name is canonicalized
fn get_map_name(map: *const libbpf_sys::bpf_map) -> Result<Option<String>> {
    let name = get_raw_map_name(map)?;

    if unsafe { !libbpf_sys::bpf_map__is_internal(map) } {
        Ok(Some(name))
    } else {
        Ok(canonicalize_internal_map_name(&name))
    }
}

fn get_prog_name(prog: *const libbpf_sys::bpf_program) -> Result<String> {
    let name_ptr = unsafe { libbpf_sys::bpf_program__name(prog) };

    if name_ptr.is_null() {
        bail!("Prog name unknown");
    }

    Ok(unsafe { CStr::from_ptr(name_ptr) }.to_str()?.to_string())
}

fn map_is_mmapable(map: *const libbpf_sys::bpf_map) -> bool {
    let def = unsafe { libbpf_sys::bpf_map__def(map) };
    (unsafe { (*def).map_flags } & libbpf_sys::BPF_F_MMAPABLE) > 0
}

fn map_is_datasec(map: *const libbpf_sys::bpf_map) -> bool {
    let internal = unsafe { libbpf_sys::bpf_map__is_internal(map) };
    let mmapable = map_is_mmapable(map);

    internal && mmapable
}

fn map_is_readonly(map: *const libbpf_sys::bpf_map) -> bool {
    assert!(map_is_mmapable(map));
    let def = unsafe { libbpf_sys::bpf_map__def(map) };

    // BPF_F_RDONLY_PROG means readonly from prog side
    (unsafe { (*def).map_flags } & libbpf_sys::BPF_F_RDONLY_PROG) > 0
}

fn open_bpf_object(name: &str, data: &[u8]) -> Result<*mut libbpf_sys::bpf_object> {
    let cname = CString::new(name)?;
    let obj_opts = libbpf_sys::bpf_object_open_opts {
        sz: std::mem::size_of::<libbpf_sys::bpf_object_open_opts>() as libbpf_sys::size_t,
        object_name: cname.as_ptr(),
        ..Default::default()
    };
    let object = unsafe {
        libbpf_sys::bpf_object__open_mem(
            data.as_ptr() as *const c_void,
            data.len() as c_ulong,
            &obj_opts,
        )
    };
    if object.is_null() {
        bail!("Failed to bpf_object__open_mem()");
    }

    Ok(object)
}

/// Generate contents of a single skeleton
fn gen_skel_contents(raw_obj_name: &str, obj_file_path: &Path) -> Result<String> {
    let mut skel = String::new();

    let libbpf_obj_name = format!("{}_bpf", raw_obj_name);
    let obj_name = capitalize_first_letter(raw_obj_name);
    let file = File::open(obj_file_path)?;
    let mmap = unsafe { Mmap::map(&file)? };
    let object = open_bpf_object(&libbpf_obj_name, &*mmap)?;
    let btf = match btf::Btf::new(&obj_name, &*mmap)? {
        Some(b) => b,
        None => bail!("failed to get btf"),
    };

    write!(
        skel,
        r#"// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
           //
           // THIS FILE IS AUTOGENERATED BY BPFSKEL!

           #[path = "bindings.rs"]
           mod bindings;
           #[path = "{name}.skel.rs"]
           mod {name}_skel;

           pub use bindings::*;
           pub use {name}_skel::*;

           #[allow(dead_code)]
           #[allow(non_snake_case)]
           #[allow(non_camel_case_types)]
           #[allow(clippy::transmute_ptr_to_ref)]
           #[allow(clippy::upper_case_acronyms)]
            use libbpf_rs::libbpf_sys;

            
            use anyhow::{{bail, Result}};
            use once_cell::sync::Lazy;
            use std::sync::Mutex;
            use std::time::Duration;
            use crate::perf::*;
        "#,
        name = raw_obj_name,
    )?;

    write!(
        skel,
        r#"
        static GLOBAL_TX: Lazy<Mutex<Option<crossbeam_channel::Sender<(usize, Vec<u8>)>>>> =
        Lazy::new(|| Mutex::new(None));

        pub fn handle_event(_cpu: i32, data: &[u8]) {{
            let event = Vec::from(data);
            GLOBAL_TX
                .lock()
                .unwrap()
                .as_ref()
                .unwrap()
                .send((_cpu as usize, event))
                .unwrap();
        }}

        pub fn handle_lost_events(cpu: i32, count: u64) {{
            eprintln!("Lost {{}} events on CPU {{}}", count, cpu);
        }}

        pub struct Skel<'a> {{
            pub skel: {name}Skel<'a>,
            rx: Option<crossbeam_channel::Receiver<(usize, Vec<u8>)>>,
        }}

        impl <'a> Default for Skel<'a> {{
            fn default() -> Self {{
                Skel {{
                    skel: unsafe {{ std::mem::MaybeUninit::zeroed().assume_init() }},
                    rx: None,
                }}
            }}
        }}

        impl<'a> Skel <'a> {{
            pub fn open(&mut self, debug: bool, btf: &Option<String>) -> Result<Open{name}Skel<'a>> {{
                let btf_cstring;
                let mut btf_cstring_ptr = std::ptr::null();
                if let Some(x) = btf {{
                    btf_cstring = std::ffi::CString::new(x.clone())?;
                    btf_cstring_ptr = btf_cstring.as_ptr();
                }}
    
                let mut skel_builder = {name}SkelBuilder::default();
                skel_builder.obj_builder.debug(debug);
                let mut open_opts = skel_builder.obj_builder.opts(std::ptr::null());
                open_opts.btf_custom_path = btf_cstring_ptr;
                let mut open_skel = skel_builder.open_opts(open_opts)?;
                Ok(open_skel)
            }}

            fn load(
                &mut self,
                mut openskel: Open{name}Skel<'a>,
                enabled: Vec<&str>,
                disabled: Vec<&str>,
            ) -> Result<()> {{
    
                if enabled.len() != 0 && disabled.len() == 0 {{
                    for x in openskel.obj.progs_iter_mut() {{
                        x.set_autoload(false)?;
                    }}
                }}
    
                for x in enabled {{
                    if let Some(y) = openskel.obj.prog_mut(x) {{
                        y.set_autoload(true)?;
                    }} else {{
                        bail!("failed to find program: {{}}", x)
                    }}
                }}
    
                for x in disabled {{
                    if let Some(y) = openskel.obj.prog_mut(x) {{
                        y.set_autoload(false)?;
                    }} else {{
                        bail!("failed to find program: {{}}", x)
                    }}
                }}
                self.skel = openskel.load()?;
                Ok(())
            }}

            fn open_load(
                &mut self,
                debug: bool,
                btf: &Option<String>,
                enabled: Vec<&str>,
                disabled: Vec<&str>,
            ) -> Result<()> {{
                let openskel = self.open(debug, btf)?;
                self.load(openskel, enabled, disabled)
            }}

            pub fn attach(&mut self) -> Result<()> {{
                Ok(self.skel.attach()?)
            }}
        
            pub fn load_enabled(&mut self, openskel: Open{name}Skel<'a>, enabled: Vec<&str>) -> Result<()> {{
                self.load(openskel, enabled, vec![])
            }}
        
            pub fn load_disabled(&mut self, openskel: Open{name}Skel<'a>, disabled: Vec<&str>) -> Result<()> {{
                self.load(openskel, vec![], disabled)
            }}
        
            pub fn open_load_enabled(
                &mut self,
                debug: bool,
                btf: &Option<String>,
                enabled: Vec<&str>,
            ) -> Result<()> {{
                self.open_load(debug, btf, enabled, vec![])
            }}
        
            pub fn open_load_disabled(
                &mut self,
                debug: bool,
                btf: &Option<String>,
                disabled: Vec<&str>,
            ) -> Result<()> {{
                self.open_load(debug, btf, vec![], disabled)
            }}
        "#,
        name = obj_name
    )?;

    for map in MapIter::new(object) {
        let map_type = unsafe { libbpf_sys::bpf_map__type(map) };
        // BPF_MAP_TYPE_PERF_EVENT_ARRAY is 4
        if map_type == 4 {
            write!(
                skel,
                r#"
                pub fn poll(&mut self, timeout: Duration) -> Result<Option<(usize, Vec<u8>)>> {{
                    if let Some(rx) = &self.rx {{
                        let data = rx.recv_timeout(timeout)?;
                        return Ok(Some(data));
                    }}
                    let (tx, rx) = crossbeam_channel::unbounded();
                    self.rx = Some(rx);
                    *GLOBAL_TX.lock().unwrap() = Some(tx);
                    let perf = PerfBufferBuilder::new(self.skel.maps_mut().perf_map())
                        .sample_cb(handle_event)
                        .lost_cb(handle_lost_events)
                        .build()?;
                    std::thread::spawn(move || loop {{
                        perf.poll(timeout).unwrap();
                    }});
                    log::debug!("start successfully perf thread to receive event");
                    Ok(None)
                }}
                "#,
            )?;
            continue;
        }

        let map_name = match get_map_name(map)? {
            Some(n) => n,
            None => continue,
        };

        if map_name.starts_with("inner") {
            // skip inner map
            continue;
        }

        let key = btf.type_declaration(unsafe { libbpf_sys::bpf_map__btf_key_type_id(map) })?;
        let value = btf.type_declaration(unsafe { libbpf_sys::bpf_map__btf_value_type_id(map) })?;

        println!("key {}, value {}", key, value);

        write!(
            skel,
            r#"
            pub fn {map_name}_update(&mut self, key: {key}, value: {value}) -> Result<()> {{
                
                let mapkey = unsafe {{ std::mem::transmute::<{key}, [u8; std::mem::size_of::<{key}>()]>(key) }};
                let mapvalue = unsafe {{ std::mem::transmute::<{value}, [u8; std::mem::size_of::<{value}>()]>(value)}};
        
                self.skel.maps_mut().{map_name}().update(
                    &mapkey,
                    &mapvalue,
                    libbpf_rs::MapFlags::ANY,
                )?;
                Ok(())
            }}

            pub fn {map_name}_delete(&mut self, key: {key}) -> Result<()> {{
                
                let mapkey = unsafe {{ std::mem::transmute::<{key}, [u8; std::mem::size_of::<{key}>()]>(key) }};
        
                self.skel.maps_mut().{map_name}().delete(
                    &mapkey,
                )?;
                Ok(())
            }}

            pub fn {map_name}_lookup(&mut self, key: {key}) -> Result<Option<{value}>> {{
                let mapkey = unsafe {{ std::mem::transmute::<{key}, [u8; std::mem::size_of::<{key}>()]>(key) }};
                
                if let Some(x) = self.skel.maps_mut().{map_name}().lookup(
                    &mapkey,
                    libbpf_rs::MapFlags::ANY,
                )? {{
                    return Ok(Some(unsafe {{ std::mem::transmute_copy::<Vec<u8>, {value}>(&x) }}));
                }}

                Ok(None)
            }}
            "#,
            map_name = map_name,
            key = key,
            value = value,
        )?;
    }

    writeln!(skel, "}}")?;

    Ok(skel)
}

/// Generate a single skeleton
fn gen_skel(name: &str, obj: &Path, out: &Path, rustfmt_path: &Path) -> Result<()> {
    if name.is_empty() {
        bail!("Object file has no name");
    }
    let skel = rustfmt(&gen_skel_contents(name, obj)?, rustfmt_path)?;
    let mut file = File::create(out)?;
    file.write_all(skel.as_bytes())?;

    Ok(())
}

pub fn gen_single(obj_file: &Path, output: &Path, rustfmt_path: &Path) -> Result<()> {
    let filename = match obj_file.file_name() {
        Some(n) => n,
        None => bail!(
            "Could not determine file name for object file: {}",
            obj_file.to_string_lossy()
        ),
    };

    let name = match filename.to_str() {
        Some(n) => {
            if !n.ends_with(".o") {
                bail!("Object file does not have `.o` suffix: {}", n);
            }

            n.split('.').next().unwrap()
        }
        None => bail!(
            "Object file name is not valid unicode: {}",
            filename.to_string_lossy()
        ),
    };

    if let Err(e) = gen_skel(name, obj_file, output, rustfmt_path) {
        bail!(
            "Failed to generate skeleton for {}: {}",
            obj_file.to_string_lossy(),
            e
        );
    }

    Ok(())
}
