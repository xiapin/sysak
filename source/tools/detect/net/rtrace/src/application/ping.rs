use crate::collector::launcher::initial_collector_thread_ping;
use crate::common::config::Config;
use crate::event::get_event_channel;
use crate::event::Event;

pub struct PingApplication {}

impl PingApplication {
    pub fn run(config: Config) {
        let (tx, rx) = get_event_channel();

        initial_collector_thread_ping(&config, tx);
        loop {
            match rx.recv() {
                Ok(event) => match event {
                    Event::Ping(p) => {
                        println!("{}", p);
                    }
                    Event::Stop => break,
                    _ => panic!("unexpected event type"),
                },
                Err(e) => panic!("unexpected channel error: {}", e),
            }
        }
    }
}
