use crate::analyzer::tcpping::TcppingAnalyzer;
use crate::analyzer::tcpping_stat::TcppingStatAnalyzer;
use crate::analyzer::virtio::VirtioAnalyzer;
use crate::collector::launcher::initial_collector_thread_tcpping;
use crate::collector::launcher::initial_collector_thread_virtio;
use crate::common::config::Config;
use crate::common::utils::ns2ms;
use crate::event::get_event_channel;
use crate::event::Event;
use std::net::Ipv4Addr;

pub struct TcppingApplication {}

impl TcppingApplication {
    pub fn run(config: Config) {
        let (tx, rx) = get_event_channel();
        let dport = config.dst.1;
        let dst = Ipv4Addr::from(u32::from_be(config.dst.0));
        println!("TCPPING {}.{}, powered by rtrace", dst, dport);

        initial_collector_thread_tcpping(&config, tx.clone());
        if config.virtio {
            initial_collector_thread_virtio(&config, tx.clone());
        }

        let mut count = 0;
        let mut tcppings = vec![];
        let mut virtios = vec![];
        loop {
            match rx.recv() {
                Ok(event) => match event {
                    Event::Tcpping(t) => {
                        log::info!("{}", t);
                        if t.is_timeout() {
                            println!("ack from {}.{}: tcp_seq={} timeout(3s)", dst, dport, t.seq,);
                        } else {
                            println!(
                                "ack from {}.{}: tcp_seq={} time={:.3}ms",
                                dst,
                                dport,
                                t.seq,
                                ns2ms(t.time())
                            );
                        }
                        tcppings.push(t);
                        count += 1;
                        if count == config.count {
                            break;
                        }
                    }
                    Event::Virtio(v) => {
                        log::info!("{}", v);
                        virtios.push(v);
                    }
                    Event::Stop => break,
                    _ => panic!("unexpected event type"),
                },
                Err(e) => panic!("unexpected channel error: {}", e),
            }
        }

        let mut virtio_diag = String::new();
        if config.virtio {
            let mut va = VirtioAnalyzer::new(virtios);
            va.analysis();
            virtio_diag = va.analysis_result();
        }

        if config.iqr {
            let mut tsa = TcppingStatAnalyzer::new(tcppings, virtio_diag);
            tsa.anaylysis();
            tsa.print(&dst, dport);
        } else {
            let mut ta = TcppingAnalyzer::new(tcppings, virtio_diag);
            ta.anaylysis();
            ta.print(&dst, dport);
        }
    }
}
