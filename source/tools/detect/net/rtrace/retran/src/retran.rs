use anyhow::{bail, Result};
use crossbeam_channel;
use std::thread;
use std::time;
use once_cell::sync::Lazy;
use builder::SkelBuilder;
use utils::macros::*;
use crate::{bindings::*, skel::*, RetranFilter, RetranEvent};
use utils::{init_zeroed, to_vecu8};
use std::sync::Mutex;
use std::time::Duration;
use utils::*;

#[derive(SkelBuilder)]
pub struct Retran<'a> {
    pub skel:RetranSkel<'a>,
    rx: Option<crossbeam_channel::Receiver<(usize, Vec<u8>)>>,
}

impl<'a> Retran<'a> {
    pub fn new(debug: bool, btf: &Option<String>) -> Result<Self> {
        let mut retran = Retran::builder().open(debug, btf).load().open_perf().build();

        // sock.skel.maps_mut().filter_map().update(
        //     &to_vec::<u32>(0),
        //     &to_vec::<inner_sock_filter>(filter),
        //     libbpf_rs::MapFlags::ANY,
        // )?;

        retran.skel.attach()?;
        Ok(retran)
    }

    pub fn poll(&mut self, timeout: Duration) -> Result<Option<RetranEvent>> {
        if let Some(rx) = &self.rx {
            match rx.recv_timeout(timeout) {
                Ok(mut data) => {
                    // https://stackoverflow.com/questions/42499049/transmuting-u8-buffer-to-struct-in-rust
                    let (head, body, _) = unsafe { data.1.align_to_mut::<retran_event>() };
                    assert!(head.is_empty(), "Data was not aligned");
                    return Ok(Some(RetranEvent::from_event(&body[0])));
                }
                Err(_) => return Ok(None),
            }
        }
        bail!("perf channel receiver is none")
    }

}



#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_basic() {
    }
}

