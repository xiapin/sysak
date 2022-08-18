#[derive(Clone, Debug)]
pub struct LogDistribution {
    /// distribution
    pub dis: [usize; 32],
}

impl Default for LogDistribution {
    fn default() -> Self {
        LogDistribution { dis: [0; 32] }
    }
}

impl std::fmt::Display for LogDistribution {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        let mut cnt = 0;
        let mut idx = 0;
        let mut max_idx = 0;

        for i in self.dis {
            cnt += i;
            if i != 0 {
                max_idx = idx;
            }
            idx += 1;
        }

        if cnt == 0 {
            cnt = 1;
            max_idx = 0;
        }

        println!(
            "{:^24}: {:<10}: {:<50}",
            "LATENCY", "FREQUENCY", "DISTRIBUTION"
        );

        idx = 0;
        for i in self.dis {
            if idx > max_idx {
                break;
            }

            let starnum = i * 50 / cnt;
            writeln!(
                f,
                "{:>10} -> {:<10}: {:<10} |{:<50}|",
                ((1 as usize) << idx) - 1,
                ((1 as usize) << (idx + 1)) - 1,
                i,
                "*".repeat(starnum)
            )?;

            idx += 1;
        }

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_logdis_display() {
        let logdis = LogDistribution::default();
        println!("{}", logdis);
    }

    #[test]
    fn test_logdis_display2() {
        let mut logdis = LogDistribution::default();
        logdis.dis[1] = 50;
        println!("{}", logdis);
    }
}
