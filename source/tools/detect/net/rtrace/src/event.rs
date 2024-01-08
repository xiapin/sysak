use crate::collector::drop::Drop;
use crate::collector::ping::Ping;
use crate::collector::queueslow::QueueSlow;
use crate::collector::retran::Retran;
use crate::collector::tcpping::Tcpping;
use crate::collector::userslow::Userslow;
use crate::collector::virtio::Virtio;
use crossbeam_channel::Receiver;
use crossbeam_channel::Sender;
use once_cell::sync::Lazy;
use std::time::Duration;

pub static GLOBAL_CHANNEL: Lazy<(Sender<Event>, Receiver<Event>)> =
    Lazy::new(|| crossbeam_channel::unbounded());

#[derive(Debug)]
pub enum Event {
    Ping(Ping),
    UserSlow(Userslow),
    QueueSlow(QueueSlow),
    Drop(Drop),
    Retran(Retran),
    Virtio(Virtio),
    Tcpping(Tcpping),
    Stop,
}

pub fn get_event_channel() -> (Sender<Event>, Receiver<Event>) {
    (GLOBAL_CHANNEL.0.clone(), GLOBAL_CHANNEL.1.clone())
}

pub fn send_stop_event() {
    if let Err(error) = GLOBAL_CHANNEL.0.send(Event::Stop) {
        log::error!("Failed to send stop event: {}", error);
    }
}

pub fn initial_stop_event_thread(t: Duration) {
    std::thread::spawn(move || {
        std::thread::sleep(t);
        send_stop_event();
    });
}
