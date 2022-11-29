use std::collections::{BTreeMap, VecDeque};

use crate::alloc_percpu_variable;

pub struct PercpuVecDeque<T> {
    lls: Vec<VecDeque<T>>,
}

impl<T> PercpuVecDeque<T>
where
    T: Default,
{
    pub fn new() -> PercpuVecDeque<T> {
        PercpuVecDeque {
            lls: alloc_percpu_variable::<VecDeque<T>>(),
        }
    }

    pub fn push(&mut self, cpu: usize, val: T) {
        self.lls[cpu].push_back(val)
    }

    // pub fn retain
}
