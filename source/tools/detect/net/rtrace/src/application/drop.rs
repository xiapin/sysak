use crate::collector::launcher::initial_collector_netdev;
use crate::collector::launcher::initial_collector_netstat;
use crate::collector::launcher::initial_collector_snmp;
use crate::collector::launcher::initial_collector_thread_drop;
use crate::common::config::Config;
use crate::event::get_event_channel;
use crate::event::Event;

pub struct DropApplication {}

impl DropApplication {
    pub fn run(config: Config) {
        let (tx, rx) = get_event_channel();
        drop_counter();

        initial_collector_thread_drop(&config, tx);

        loop {
            match rx.recv() {
                Ok(event) => match event {
                    Event::Drop(d) => {
                        println!("{}", serde_json::to_string(&d).unwrap());
                    }
                    Event::Stop => break,
                    _ => panic!("unexpected event type"),
                },
                Err(e) => panic!("unexpected channel error: {}", e),
            }
        }

        drop_counter();
    }
}

fn drop_counter() {
    let netstat = initial_collector_netstat().unwrap();
    let snmp = initial_collector_snmp().unwrap();
    let netdev = initial_collector_netdev().unwrap();

    println!("{}", serde_json::to_string(&netstat).unwrap());
    println!("{}", serde_json::to_string(&snmp).unwrap());
    println!("{}", serde_json::to_string(&netdev).unwrap());
}
