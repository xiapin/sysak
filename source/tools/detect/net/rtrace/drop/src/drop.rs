use crate::{bindings::*, skel::*, DropFilter, DropEvent};
use anyhow::{bail, Result};
use builder::SkelBuilder;
use crossbeam_channel;
use once_cell::sync::Lazy;
use std::sync::Mutex;
use std::thread;
use std::time;
use std::time::Duration;
use utils::macros::*;
use utils::{init_zeroed, to_vec, kernel_stack::KernelStack};

#[derive(SkelBuilder)]
pub struct Drop<'a> {
    pub skel: DropSkel<'a>,
    rx: Option<crossbeam_channel::Receiver<(usize, Vec<u8>)>>,
}

impl<'a> Drop<'a> {
    pub fn new(debug: bool, btf: &Option<String>, filter: DropFilter) -> Result<Self> {
        let mut drop = Drop::builder().open(debug, btf).load().open_perf().build();

        drop.skel.maps_mut().filter_map().update(
            &to_vec::<u32>(0),
            &filter.to_vec(),
            libbpf_rs::MapFlags::ANY,
        )?;

        drop.skel.attach()?;
        Ok(drop)
    }

    pub fn poll(&mut self, timeout: Duration) -> Result<Option<DropEvent>> {
        if let Some(rx) = &self.rx {
            match rx.recv_timeout(timeout) {
                Ok(mut data) => {
                    // https://stackoverflow.com/questions/42499049/transmuting-u8-buffer-to-struct-in-rust
                    let (head, body, tail) = unsafe { data.1.align_to_mut::<drop_event>() };
                    assert!(head.is_empty(), "Data was not aligned");

                    let mut se = body[0];
                    let mut stack = self.get_stack(se.stackid);
                    let mut de = DropEvent::from_drop_event(se);
                    
                    match stack {
                        Ok(s) => de.set_stack(s),
                        Err(_) => {}
                    }
                    return Ok(Some(de));
                }
                Err(_) => return Ok(None),
            }
        }
        bail!("perf channel receiver is none")
    }

    pub fn get_stack(&mut self, stackid: u32) -> Result<Vec<u8>> {
        let stack = self.skel.maps_mut().stackmap().lookup(&utils::to_vec::<u32>(stackid), libbpf_rs::MapFlags::ANY)?;

        if let Some(s) = stack {
            return Ok(s);
        }
        bail!("failed to find stack")
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_events_sock_basic() {
        utils::bump_memlock_rlimit().unwrap();
        let mut sock = Sock::new(true, &None);
        loop {
            if let Some(event) = sock.poll(Duration::from_millis(200)).unwrap() {
                println!("{}", event);
            }
        }
    }
}
