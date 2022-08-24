#[path = "bpf/.output/skel.rs"]
mod skel;
use skel::*;




// pub struct TcpConnect {
//     skel: Box<TcpconnectSkel>,
//     pub a: u32,
// }


pub struct Event {

}

impl Event {
    pub fn new(data: (usize, Vec<u8>)) -> Event{
        Event {

        }
    }
}

