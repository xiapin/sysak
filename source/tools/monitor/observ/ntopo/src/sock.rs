use std::cmp::Ordering;
use std::io::Read;
use std::os::unix::net::UnixDatagram;
use std::os::unix::net::UnixListener;

use libbpf_rs::MapHandle;

pub fn unix_sock_send(addr: &str, data: &String) {
    match UnixDatagram::bind(addr) {
        Ok(sock) => {
            if let Err(e) = sock.send(&data.as_bytes()) {
                println!("failed to send data to sock: {addr}, error: {e}, data: {data}");
            }
        }
        Err(e) => {
            println!("failed to connnect to sock: {addr}, error: {e}, data: {data}");
        }
    }
}

pub fn unix_sock_recv(pidsmap: &MapHandle, addr: &str) {
    let listen = UnixListener::bind(addr).expect("failed to listen unix sock");

    for stream in listen.incoming() {
        let mut stream = stream.unwrap();

        let mut recvstring = String::new();
        stream.read_to_string(&mut recvstring).unwrap();

        let slices: Vec<&str> = recvstring.split(",").collect();

        if slices.len() == 0 {
            continue;
        }

        let mut pid_array = vec![];

        if slices[0].cmp("mysql") == Ordering::Equal {
            for pid in &slices[1..] {
                let pid_num: u32 = pid.parse::<u32>().unwrap();
                pid_array.push(pid_num);
            }

            let len = pid_array[0] as usize;
            if len != pid_array.len() - 1 {
                println!("data format is not right");
            } else {
                for key in pidsmap.keys() {
                    pidsmap.delete(&key).expect("failed to delete pidsmap key");
                }

                let val = vec![0; std::mem::size_of::<crate::pid_info>()];

                for pid in &pid_array[1..] {
                    pidsmap
                        .update(&pid.to_ne_bytes(), &val, libbpf_rs::MapFlags::NO_EXIST)
                        .expect("failed to update pidsmap");
                }
            }
        }
    }
}
