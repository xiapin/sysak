pub mod macros;
pub mod perf;
pub mod timestamp;

mod drop;
pub use {drop::delta_dev, drop::delta_netstat, drop::delta_snmp};

pub mod pstree;

pub mod net;
use anyhow::{bail, Result};

pub mod kernel_stack;
pub mod percpu_queue;
// pub mod process;

pub fn to_vec<T>(t: T) -> Vec<u8> {
    unsafe {
        std::slice::from_raw_parts(&t as *const T as *const u8, std::mem::size_of::<T>()).to_vec()
    }
}

pub fn bump_memlock_rlimit() -> Result<()> {
    let rlimit = libc::rlimit {
        rlim_cur: 128 << 20,
        rlim_max: 128 << 20,
    };

    if unsafe { libc::setrlimit(libc::RLIMIT_MEMLOCK, &rlimit) } != 0 {
        bail!("Failed to increase rlimit");
    }

    Ok(())
}

pub fn cpus_number() -> usize {
    num_cpus::get()
}

pub fn alloc_percpu_variable<T>() -> Vec<T>
where
    T: Default,
{
    let mut ret = vec![];
    for _ in 0..cpus_number() {
        ret.push(T::default())
    }
    ret
}

pub fn kernel_version() -> Result<String> {
    let mut info = unsafe { std::mem::MaybeUninit::<libc::utsname>::zeroed().assume_init() };
    let mut release_version = Vec::with_capacity(info.release.len());
    let ret = unsafe { libc::uname(&mut info as *mut libc::utsname) };
    if ret < 0 {
        bail!("failed to call function: libc::uname, error code: {}", ret)
    }

    for i in info.release {
        release_version.push(i as u8);
    }
    Ok(String::from_utf8(release_version)?)
}
