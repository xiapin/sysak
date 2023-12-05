/// raw information of perf event
pub struct RawEvent {
    pub cpu: i32,
    pub ty: u32,
    pub data: Vec<u8>,
}
