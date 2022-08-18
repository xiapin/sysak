use crate::common::*;
use crate::latency::bindings::*;
use crate::latency::skel::*;
use crate::utils::LogDistribution;
use anyhow::{bail, Result};
use crossbeam_channel;
use crossbeam_channel::Receiver;
use libbpf_rs::Map;
use libbpf_rs::MapFlags;
use std::ffi::CString;

fn open_load_skel<'a>(debug: bool, btf: &Option<String>) -> Result<LatencySkel<'a>> {
    let btf_cstring;
    let mut btf_cstring_ptr = std::ptr::null();
    if let Some(btf) = btf {
        btf_cstring = CString::new(btf.clone())?;
        btf_cstring_ptr = btf_cstring.as_ptr();
    }

    let mut skel_builder = LatencySkelBuilder::default();
    skel_builder.obj_builder.debug(debug);
    let mut open_opts = skel_builder.obj_builder.opts(std::ptr::null());
    open_opts.btf_custom_path = btf_cstring_ptr;
    let mut open_skel = skel_builder.open_opts(open_opts)?;
    Ok(open_skel.load()?)
}

pub struct Latency<'a> {
    skel: LatencySkel<'a>,
    pub filter: Filter,
    zerohist: Loghist,
}

impl<'a> Latency<'a> {
    pub fn new(debug: bool, btf: &Option<String>) -> Result<Latency<'a>> {
        let skel = open_load_skel(debug, btf)?;
        Ok(Latency {
            skel,
            filter: Filter::new(),
            zerohist: Loghist::zero(),
        })
    }

    pub fn get_loghist(&mut self) -> Result<Option<Loghist>> {
        let mut key = vec![0; self.skel.maps().hists().key_size() as usize];

        let res = self.skel.maps_mut().hists().lookup(&key, MapFlags::ANY)?;
        if let Some(x) = res {
            return Ok(Some(Loghist::new(x)));
        }
        self.skel
            .maps_mut()
            .hists()
            .update(&key, &self.zerohist.vec(), MapFlags::ANY)?;
        Ok(None)
    }

    pub fn attach(&mut self) -> Result<()> {
        Ok(self.skel.attach()?)
    }
}

pub struct Loghist {
    lh: loghist,
}

impl Loghist {
    pub fn zero() -> Loghist {
        Loghist {
            lh: unsafe { std::mem::MaybeUninit::zeroed().assume_init() },
        }
    }

    pub fn new(data: Vec<u8>) -> Loghist {
        let mut lh = unsafe { std::mem::MaybeUninit::zeroed().assume_init() };
        let ptr = &data[0] as *const u8 as *const loghist;

        unsafe {
            std::ptr::copy_nonoverlapping(ptr, &mut lh, 1);
        }

        Loghist { lh }
    }

    pub fn to_logdistribution(&self) -> LogDistribution {
        let mut logdis = LogDistribution::default();

        let mut idx = 0;
        for i in self.lh.hist {
            logdis.dis[idx] = i as usize;
            idx += 1;
        }

        logdis
    }

    pub fn vec(&self) -> Vec<u8> {
        unsafe {
            std::slice::from_raw_parts(
                &self.lh as *const loghist as *const u8,
                std::mem::size_of::<loghist>(),
            )
            .to_vec()
        }
    }
}
