use super::*;

#[derive(Debug, Default)]
pub struct SynQueue {
    len: u32,
    max: u32,
}

impl DropReason for SynQueue {}
