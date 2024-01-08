use libbpf_rs::Map;
use libbpf_rs::MapHandle;

use crate::common::utils::any_as_u8_slice;
mod bpf {
    include!(concat!(env!("OUT_DIR"), "/filter.rs"));
}

pub struct Filter {
    map: MapHandle,
    raw: bpf::filter,
}

impl Filter {
    pub fn new(map: &Map) -> Self {
        Filter {
            map: libbpf_rs::MapHandle::try_clone(map).unwrap(),
            raw: unsafe { std::mem::zeroed::<bpf::filter>() },
        }
    }

    pub fn set_threshold(&mut self, t: u64) {
        self.raw.threshold = t;
    }

    pub fn clear(&mut self) {
        self.raw = unsafe { std::mem::zeroed::<bpf::filter>() }
    }

    pub fn update(&mut self) {
        self.map
            .update(
                &0_u32.to_ne_bytes(),
                unsafe { any_as_u8_slice(&self.raw) },
                libbpf_rs::MapFlags::ANY,
            )
            .expect("failed to update filter map");
    }
}
