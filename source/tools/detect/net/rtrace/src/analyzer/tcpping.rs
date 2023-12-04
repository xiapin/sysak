use crate::collector::tcpping::Tcpping;
use crate::collector::tcpping::TcppingStage;
use crate::common::sched::Process;
use crate::common::sched::Sched;
use crate::common::stats::Stats;
use crate::common::utils::ns2ms;
use std::net::Ipv4Addr;

#[derive(Debug, Default)]
struct TcppingStat {
    seq: u32,
    timeout: bool,
    // analysis
    time_tot: u64,
    time_user_tx: u64,
    time_user_rx: u64,
    time_kernel_tx: u64,
    time_kernel_rx: u64,
    time_external_link: u64,

    // ksoftirqd
    in_ksoftirqd: bool,
    xmit_irq: u64,
    sched_irq: u64,
    process: Process,
}

impl From<&Tcpping> for TcppingStat {
    fn from(tp: &Tcpping) -> Self {
        let mut tstat = TcppingStat::default();
        tstat.seq = tp.seq;

        if tp.is_timeout() {
            tstat.timeout = true;
            return tstat;
        }

        let xmit = tp.stage_ts(TcppingStage::TxKernelOut);
        let recv = tp.stage_ts(TcppingStage::RxKernelIn);

        tstat.time_tot = tp.delta(TcppingStage::RxUser, TcppingStage::TxUser);
        tstat.time_user_tx = tp.delta(TcppingStage::TxKernelIn, TcppingStage::TxUser);
        tstat.time_user_rx = tp.delta(TcppingStage::RxUser, TcppingStage::RxKernelOut);
        tstat.time_kernel_tx = tp.delta(TcppingStage::TxKernelOut, TcppingStage::TxKernelIn);
        tstat.time_kernel_rx = tp.delta(TcppingStage::RxKernelOut, TcppingStage::RxKernelIn);
        tstat.time_external_link = recv - xmit;

        let in_range = |left, right, cur| {
            if cur >= left && cur <= right {
                true
            } else {
                false
            }
        };

        let scheds: Vec<(&u64, &Sched)> = tp.scheds().collect();
        if let Some(sched) = scheds.last() {
            if in_range(xmit, recv, *sched.0) && sched.1.next.comm.starts_with("ksoftirqd") {
                tstat.in_ksoftirqd = true;
            }
        }

        if tstat.in_ksoftirqd {
            for (i, (&ts, sched)) in scheds.iter().enumerate() {
                if in_range(xmit, recv, ts) {}
            }
        }

        let irqs_iter = tp.irqs();
        if let Some(&irq) = irqs_iter.last() {
            if irq > xmit {
                tstat.xmit_irq = irq - xmit;

                for (&ts, sched) in scheds.iter().rev() {
                    if ts < irq {
                        tstat.process = sched.next.clone();
                        tstat.sched_irq = irq - ts;
                        break;
                    }
                }
            }
        }

        tstat
    }
}

#[derive(Debug, Default)]
pub struct TcppingAnalyzer {
    // raw
    tpstats: Vec<TcppingStat>,
    // base
    packet_loss: u32,
    packet_done: u32,
    base_stats: Stats,
    // enhanced
    tx_user_stats: Stats,
    tx_kernel_stats: Stats,
    external_link_stats: Stats,
    rx_kernel_stats: Stats,
    rx_user_stats: Stats,
    // diagnose
    diags: Vec<String>,
}

macro_rules! collect_field {
    ($st: expr, $field: ident) => {
        $st.iter()
            .filter(|x| !x.timeout)
            .map(|x| x.$field)
            .collect()
    };
}

impl TcppingAnalyzer {
    pub fn new(tps: Vec<Tcpping>, virtio: String) -> Self {
        let mut ta = TcppingAnalyzer::default();
        let tpstats: Vec<TcppingStat> = tps.iter().map(|tp| TcppingStat::from(tp)).collect();
        ta.tpstats = tpstats;
        if !virtio.is_empty() {
            ta.diags.push(virtio);
        }
        ta
    }

    fn packet_count(&mut self) {
        let mut packet_loss = 0;
        let mut packet_done = 0;

        self.tpstats.iter().for_each(|x| {
            if x.timeout {
                packet_loss += 1;
            } else {
                packet_done += 1;
            }
        });
        self.packet_done = packet_done;
        self.packet_loss = packet_loss;
    }

    pub fn anaylysis(&mut self) {
        // base
        self.packet_count();
        if self.packet_done == 0 {
            return;
        }
        let tot: Vec<u64> = collect_field!(self.tpstats, time_tot);
        self.base_stats = Stats::new(tot);

        // enhanced
        let tx_user = collect_field!(self.tpstats, time_user_tx);
        self.tx_user_stats = Stats::new(tx_user);

        let tx_kernel = collect_field!(self.tpstats, time_kernel_tx);
        self.tx_kernel_stats = Stats::new(tx_kernel);

        let external_link = collect_field!(self.tpstats, time_external_link);
        self.external_link_stats = Stats::new(external_link);

        let rx_kernel = collect_field!(self.tpstats, time_kernel_rx);
        self.rx_kernel_stats = Stats::new(rx_kernel);

        let rx_user = collect_field!(self.tpstats, time_user_rx);
        self.rx_user_stats = Stats::new(rx_user);

        // diagnose
        let is_overlimit = |avg: u64, cur: u64| {
            if cur < avg {
                return false;
            }
            if cur - avg > 500_000 && (cur - avg) * 100 / avg >= 50 {
                return true;
            }
            false
        };

        let mut diags = vec![];
        for tp in &self.tpstats {
            if tp.timeout {
                continue;
            }
            let seq_string = format!("tcp_seq={} time={:.3}ms", tp.seq, ns2ms(tp.time_tot));
            if is_overlimit(self.tx_user_stats.avg, tp.time_user_tx) {
                diags.push(format!(
                    "{seq_string}, reason: Sending packet in user mode is too slow"
                ));
            }

            if is_overlimit(self.tx_kernel_stats.avg, tp.time_kernel_tx) {
                diags.push(format!(
                    "{seq_string}, reason: Sending packet in kernel mode is too slow"
                ));
            }

            // if is_overlimit(out.avg, tp.time_external_link) {
            //     println!("seq: {}, reason: Sending packet in kernel mode is too slow", tp.seq);
            // }
            // detect irq off
            if is_overlimit(self.external_link_stats.avg, tp.xmit_irq) {
                if !tp.process.comm.contains("swapper")
                    && (tp.xmit_irq < tp.sched_irq * 2 && tp.sched_irq > 1_000_000)
                {
                    diags.push(format!("{seq_string}, reason: may be that process {} has turned off interrupts. So far, this process has occupied {}ms of cpu.", tp.process.to_string(), ns2ms(tp.sched_irq)));
                } else {
                    diags.push(format!("{seq_string}, reason: external link is too slow"));
                }
            }

            if is_overlimit(self.rx_kernel_stats.avg, tp.time_kernel_rx) {
                diags.push(format!(
                    "{seq_string}, reason: Receiving packet in kernel mode is too slow",
                ));
            }

            if is_overlimit(self.rx_user_stats.avg, tp.time_user_rx) {
                diags.push(format!(
                    "{seq_string}, reason: Receiving packet in user mode is too slow",
                ));
            }
        }
        self.diags.extend_from_slice(&diags);
    }

    fn stats_string(&self, s: &Stats) -> String {
        format!(
            "min/avg/max/mdev = {:.3}/{:.3}/{:.3}/{:.3} ms",
            ns2ms(s.min),
            ns2ms(s.avg),
            ns2ms(s.max),
            ns2ms(s.mdev)
        )
    }

    pub fn base_string(&self, dst: &Ipv4Addr, dport: u16) -> String {
        let mut lines = vec![];
        lines.push(format!("--- {}.{} tcpping statistics ---", dst, dport));
        lines.push(format!(
            "{} packets transmitted, {} received, {:.2}% packet loss",
            self.packet_done + self.packet_loss,
            self.packet_done,
            (self.packet_loss * 100) as f32 / (self.packet_done + self.packet_loss) as f32
        ));
        lines.push(self.stats_string(&self.base_stats));
        lines.join("\n")
    }

    pub fn enhanced_string(&self, dst: &Ipv4Addr, dport: u16) -> String {
        let mut lines = vec![];
        lines.push(format!(
            "--- {}.{} tcpping enhanced statistics ---",
            dst, dport
        ));

        lines.push(format!(
            "userspace transmitted time consuming {}",
            self.stats_string(&self.tx_user_stats)
        ));
        lines.push(format!(
            "kernel    transmitted time consuming {}",
            self.stats_string(&self.tx_kernel_stats)
        ));
        lines.push(format!(
            "link+irq+softirq      time consuming {}",
            self.stats_string(&self.external_link_stats)
        ));
        lines.push(format!(
            "kernel       received time consuming {}",
            self.stats_string(&self.rx_kernel_stats)
        ));
        lines.push(format!(
            "userspace    received time consuming {}",
            self.stats_string(&self.rx_user_stats)
        ));
        lines.join("\n")
    }

    pub fn diagnose_string(&self, dst: &Ipv4Addr, dport: u16) -> String {
        let mut lines = vec![];
        lines.push(format!(
            "--- {}.{} tcpping diagnosing report ---",
            dst, dport
        ));
        if self.diags.len() == 0 {
            lines.push("everything is ok!".to_owned());
        } else {
            lines.extend_from_slice(&self.diags);
        }
        lines.join("\n")
    }

    pub fn analysis_result(&self, dst: &Ipv4Addr, dport: u16) -> String {
        let base = self.base_string(dst, dport);
        let en = self.enhanced_string(dst, dport);
        let diag = self.diagnose_string(dst, dport);

        format!("\n{}\n\n{}\n\n{}", base, en, diag)
    }

    pub fn print(&self, dst: &Ipv4Addr, dport: u16) {
        println!("{}", self.analysis_result(dst, dport));
    }
}
