use anyhow::Result;
use byteorder::{NativeEndian, ReadBytesExt};
use once_cell::sync::Lazy;
use std::collections::HashSet;
use std::fs::File;
use std::io::Cursor;
use std::io::{self, BufRead};
use std::sync::Mutex;

pub static GLOBAL_KALLSYMS: Lazy<Mutex<Kallsyms>> = Lazy::new(|| {
    let ksyms = Kallsyms::try_from("/proc/kallsyms").unwrap();
    Mutex::new(ksyms)
});

#[derive(Debug, Default)]
pub struct Kallsyms {
    syms: Vec<(String, u64)>,
    hs: HashSet<String>,
}

impl TryFrom<&str> for Kallsyms {
    type Error = anyhow::Error;
    fn try_from(path: &str) -> Result<Self> {
        let mut ksyms = Kallsyms::new();
        let file = File::open(path)?;
        let lines = io::BufReader::new(file).lines();
        for line in lines {
            if let Ok(l) = line {
                let mut iter = l.trim().split_whitespace();
                if let Some(x) = iter.next() {
                    iter.next();
                    if let Some(y) = iter.next() {
                        ksyms.insert(y.to_string(), u64::from_str_radix(x, 16)?);
                    }
                }
            }
        }
        ksyms.sort();
        log::debug!(
            "Load ksyms done from {:?}, symbols length: {}",
            path,
            ksyms.get_ksyms_num()
        );
        Ok(ksyms)
    }
}

impl Kallsyms {
    pub fn new() -> Self {
        Kallsyms {
            syms: Vec::new(),
            hs: HashSet::default(),
        }
    }

    fn insert(&mut self, sym_name: String, sym_addr: u64) {
        self.syms.push((sym_name.clone(), sym_addr));
        self.hs.insert(sym_name);
    }

    fn get_ksyms_num(&self) -> usize {
        self.syms.len()
    }

    fn sort(&mut self) {
        self.syms.sort_by(|a, b| a.1.cmp(&b.1));
    }

    pub fn has_sym(&self, sym_name: &str) -> bool {
        self.hs.contains(sym_name)
    }

    pub fn addr_to_sym(&self, addr: u64) -> String {
        let mut start = 0;
        let mut end = self.syms.len() - 1;
        let mut mid;
        let mut sym_addr;

        while start < end {
            mid = start + (end - start + 1) / 2;
            sym_addr = self.syms[mid].1;

            if sym_addr <= addr {
                start = mid;
            } else {
                end = mid - 1;
            }
        }

        if start == end && self.syms[start].1 <= addr {
            let mut name = self.syms[start].0.clone();
            name.push_str(&format!("+{}", addr - self.syms[start].1));
            return name;
        }

        return String::from("Not Found");
    }
}

pub struct KernelStack {
    stack: Vec<String>,
}

impl KernelStack {
    pub fn new(stack: &Vec<u8>) -> KernelStack {
        let depth = stack.len() / 8;
        let mut rdr = Cursor::new(stack.clone());
        let mut stack_str = Vec::new();
        let kallsyms = GLOBAL_KALLSYMS.lock().unwrap();

        for _ in 0..depth {
            let addr = rdr.read_u64::<NativeEndian>().unwrap();
            stack_str.push(kallsyms.addr_to_sym(addr));
        }
        KernelStack { stack: stack_str }
    }
}

impl std::fmt::Display for KernelStack {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        for s in &self.stack {
            writeln!(f, "\t{}", s)?;
        }
        write!(f, "")
    }
}


pub fn has_kernel_symbol(name: &str) -> bool {
    GLOBAL_KALLSYMS.lock().unwrap().has_sym(name)
}