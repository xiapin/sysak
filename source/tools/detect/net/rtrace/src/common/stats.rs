use ndarray::Array1;
use ndarray_stats::QuantileExt;
use noisy_float::prelude::*;

#[derive(Debug, Default)]
pub struct Stats {
    pub min: u64,
    pub avg: u64,
    pub max: u64,
    pub mdev: u64,
    pub stddev: u64,
}

impl Stats {
    pub fn new(a: Vec<u64>) -> Self {
        let arr = Array1::from_vec(a);
        let avg = arr.mean().unwrap();
        let mdev = arr.mapv(|x| x.abs_diff(avg)).mean().unwrap() as u64;
        let variance = arr.mapv(|x| x.abs_diff(avg).pow(2)).sum() / (arr.len() as u64);

        Stats {
            min: *arr.min().unwrap(),
            avg,
            max: *arr.max().unwrap(),
            mdev,
            stddev: f64::sqrt(variance as f64) as u64,
        }
    }

    pub fn zscore(&self, v: u64) -> u64 {
        if v < self.avg {
            return 0;
        }
        (v - self.avg) / self.stddev
    }
}
