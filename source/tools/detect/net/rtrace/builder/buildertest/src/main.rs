
use builder::SkelBuilder;

use anyhow::{bail, Result};
use libbpf_rs::libbpf_sys;
use once_cell::sync::Lazy;
use std::collections::HashMap;
use std::sync::Mutex;
use std::time::Duration;

struct IcmpSkel<'a> {
    a: &'a HashMap<i32, i32>,
}

#[derive(SkelBuilder)]
pub struct Test<'a> {
    pub skel: IcmpSkel<'a>,
    rx: Option<crossbeam_channel::Receiver<(usize, Vec<u8>)>>,
    delta: u64,
    events: HashMap<i32, i32>,
}



fn main() {
 
    
}