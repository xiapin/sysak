use crate::collector::launcher::initial_collector_thread_retran;
use crate::common::config::Config;
use crate::event::get_event_channel;
use crate::event::Event;

pub struct RetranApplication {}

impl RetranApplication {
    pub fn run(config: Config) {
        let (tx, rx) = get_event_channel();

        initial_collector_thread_retran(&config, tx);

        loop {
            match rx.recv() {
                Ok(event) => match event {
                    Event::Retran(r) => {
                        println!("{}", serde_json::to_string(&r).unwrap());
                    }
                    Event::Stop => break,
                    _ => panic!("unexpected event type"),
                },
                Err(e) => panic!("unexpected channel error: {}", e),
            }
        }
    }
}
