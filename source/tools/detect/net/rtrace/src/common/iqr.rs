use ndarray::Array;
use ndarray::Array1;
use ndarray_stats::interpolate::Nearest;
use ndarray_stats::Quantile1dExt;
use noisy_float::types::n64;

pub struct IQR {
    q1: u64,
    q3: u64,
    upper: u64,
}

impl IQR {
    pub fn new(mut array: Array<u64, ndarray::Dim<[usize; 1]>>) -> Self {
        let q1 = array.quantile_mut(n64(0.25), &Nearest).unwrap();
        let q3 = array.quantile_mut(n64(0.75), &Nearest).unwrap();
        let iqr = q3 - q1;
        let upper = (iqr as f64 * 1.5) as u64 + q3;
        IQR { q1, q3, upper }
    }

    pub fn is_upper_outlier(&self, val: u64) -> bool {
        if val > self.upper {
            true
        } else {
            false
        }
    }
}

pub fn iqr_upper_outliers(data: Vec<u64>) -> Vec<usize> {
    let arr = Array1::from_vec(data.clone());
    let iqr = IQR::new(arr);

    let mut res = vec![];
    for (i, &d) in data.iter().enumerate() {
        if iqr.is_upper_outlier(d) {
            res.push(i);
        }
    }
    res
}
