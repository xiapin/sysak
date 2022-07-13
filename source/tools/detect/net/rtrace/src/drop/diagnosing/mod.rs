use super::event::Event;
use eutils_rs::proc::Snmp;

mod synqueue;
pub use self::synqueue::SynQueue;

use super::DropParams;

pub trait DropReason {
    fn init(&self, _: &DropParams) {}
    fn probability(&self) -> usize {
        0
    }
    fn display(&self) -> String {
        "无相关诊断数据显示，请联系开发者支持".to_owned()
    }
    fn reason(&self) -> String {
        "无法诊断丢包原因，请联系开发者支持".to_owned()
    }
    fn recommend(&self) -> String {
        "暂无诊断建议，请联系开发者支持".to_owned()
    }
    
}

#[derive(Default)]
pub struct UnspecifiedDropReason {}

impl DropReason for UnspecifiedDropReason {
    fn probability(&self) -> usize {
        90
    }
}



pub fn build_drop_reasons() -> Vec<Box<dyn DropReason>> {
    let mut drs: Vec<Box<dyn DropReason>> = Vec::new();

    drs.push(Box::new(UnspecifiedDropReason::default()));
    drs.push(Box::new(SynQueue::default()));

    drs
}
