use crate::collector::launcher::initial_collector_thread_queueslow;
use crate::collector::launcher::initial_collector_thread_userslow;
use crate::common::config::Config;
use crate::event::get_event_channel;
use crate::event::Event;

pub struct JitterApplication {}

impl JitterApplication {
    pub fn run(config: Config) {
        let (tx, rx) = get_event_channel();

        initial_collector_thread_queueslow(&config, tx.clone());
        initial_collector_thread_userslow(&config, tx);

        loop {
            match rx.recv() {
                Ok(event) => match event {
                    Event::QueueSlow(q) => {
                        if config.output_json {
                            println!("{}", serde_json::to_string(&q).unwrap());
                        } else {
                            println!("{}", q);
                        }
                    }
                    Event::UserSlow(u) => {
                        if config.output_json {
                            println!("{}", serde_json::to_string(&u).unwrap());
                        } else {
                            println!("{}", u);
                        }
                    }
                    Event::Stop => break,
                    _ => panic!("unexpected event type"),
                },
                Err(e) => panic!("unexpected channel error: {}", e),
            }
        }
    }
}
