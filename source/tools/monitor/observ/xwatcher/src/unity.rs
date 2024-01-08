use chrono::prelude::*;
use std::os::unix::net::UnixDatagram;

pub fn unity_sock_send(addr: &str, data: &String) {
    if data.len() == 0 {
        return;
    }
    let sock = UnixDatagram::unbound().expect("failed to create unix sock");
    log::debug!("send message to unity: {}", data);
    match sock.connect(addr) {
        Ok(()) => {
            if let Err(e) = sock.send(&data.as_bytes()) {
                println!("failed to send data to sock: {addr}, error: {e}, data: {data}");
            }
        }
        Err(e) => {
            println!("failed to connnect to sock: {addr}, error: {e}, data: {data}");
        }
    }
}

pub fn unity_sock_send_event(addr: &str, extra: &String) {
    let ts = Utc::now().timestamp();
    let data = format!(
        "node_event event_type=\"metric_exception\",description=\"nginx\",extra={:?},ts={:?}",
        extra, ts
    );

    unity_sock_send(addr, &data);
}
