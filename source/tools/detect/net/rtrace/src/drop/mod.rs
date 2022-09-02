#[path = "bpf/.output/skel.rs"]
mod skel;

mod drop;

pub use {self::drop::build_drop, self::drop::Drop, self::drop::DropCommand};
