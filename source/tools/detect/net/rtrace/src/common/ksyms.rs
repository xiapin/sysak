use anyhow::Result;
use once_cell::sync::Lazy;
use std::collections::BTreeMap;
use std::collections::HashMap;
use std::fs::File;
use std::io::BufRead;
use std::io::{self};
use std::ops::Bound;

pub static GLOBAL_KALLSYMS: Lazy<KSyms> = Lazy::new(|| {
    let ksyms = KSyms::try_from("/proc/kallsyms").unwrap();
    ksyms
});

#[derive(Debug, Default)]
pub struct KSyms {
    hash: HashMap<String, u64>,
    syms: BTreeMap<u64, String>,
}

impl TryFrom<&str> for KSyms {
    type Error = anyhow::Error;
    fn try_from(path: &str) -> Result<Self> {
        let mut ksyms = KSyms::new();
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
        log::debug!(
            "Load ksyms done from {:?}, symbols length: {}",
            path,
            ksyms.ksyms_size()
        );
        Ok(ksyms)
    }
}

impl KSyms {
    pub(crate) fn new() -> Self {
        KSyms {
            syms: BTreeMap::new(),
            hash: HashMap::default(),
        }
    }

    fn insert(&mut self, sym_name: String, sym_addr: u64) {
        self.syms.insert(sym_addr, sym_name.clone());
        self.hash.insert(sym_name, sym_addr);
    }

    fn ksyms_size(&self) -> usize {
        self.syms.len()
    }

    pub fn has_sym(&self, sym_name: &str) -> bool {
        self.hash.contains_key(sym_name)
    }
}

pub fn has_kernel_symbol(name: &str) -> bool {
    GLOBAL_KALLSYMS.has_sym(name)
}

pub fn get_symbol(addr: &u64) -> &String {
    let res = GLOBAL_KALLSYMS
        .syms
        .range((Bound::Unbounded, Bound::Included(addr)));

    res.last().unwrap().1
}

pub fn get_symbol_with_offset(addr: &u64) -> String {
    let res = GLOBAL_KALLSYMS
        .syms
        .range((Bound::Unbounded, Bound::Included(addr)));

    let last = res.last().unwrap();
    format!("{}+{}", last.1, addr - last.0)
}

pub fn get_addr(sym: &str) -> Option<&u64> {
    GLOBAL_KALLSYMS.hash.get(sym)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn ksyms() {
        let ksyms = KSyms::try_from("/proc/kallsyms").unwrap();
        assert_ne!(ksyms.ksyms_size(), 0);
        assert!(ksyms.has_sym("tcp_sendmsg"));
    }

    #[test]
    fn global_ksyms() {
        assert!(has_kernel_symbol("tcp_sendmsg"));

        let addr = get_addr("tcp_sendmsg").unwrap();
        assert_eq!(get_symbol(addr), "tcp_sendmsg");

        let new_addr = *addr + 53;
        assert_eq!(get_symbol_with_offset(&new_addr), "tcp_sendmsg+53");
    }
}
