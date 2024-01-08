use std::collections::BTreeMap;
use std::collections::BTreeSet;
use std::ops::Bound::Excluded;
use std::ops::Bound::Included;
use std::ops::Bound::Unbounded;

/// wrapper for Vec<BTreeSet<T>>
pub struct CpuBTreeSet<T: Clone>(Vec<BTreeSet<T>>);

impl<T> CpuBTreeSet<T>
where
    T: Clone,
{
    pub fn new(cpu: usize) -> Self {
        CpuBTreeSet(vec![BTreeSet::<T>::new(); cpu])
    }

    pub fn insert(&mut self, cpu: usize, value: T)
    where
        T: Ord,
    {
        self.0[cpu].insert(value);
    }

    pub fn in_range(&mut self, cpu: usize, left: T, right: T) -> Vec<T>
    where
        T: Ord,
    {
        let l = Included(left);
        let r = Included(right);
        let mut res = vec![];
        for elem in self.0[cpu].range((l, r)) {
            res.push(elem.clone());
        }
        res
    }

    pub fn lower_bound(&self, cpu: usize, val: T) -> Option<&T>
    where
        T: Ord,
    {
        let mut res = self.0[cpu].range((Unbounded, Excluded(val)));
        res.next_back()
    }

    pub fn upper_bound(&self, cpu: usize, val: T) -> Option<&T>
    where
        T: Ord,
    {
        let mut res = self.0[cpu].range((Excluded(val), Unbounded));
        res.next()
    }

    /// Delete old data
    pub fn flush(&mut self, cpu: usize, val: T)
    where
        T: Ord,
    {
        let new = self.0[cpu].split_off(&val);
        self.0[cpu] = new;
    }
}

/// wrapper for Vec<BTreeMap<K, V>>
pub struct CpuBTreeMap<K: Clone, V: Clone>(Vec<BTreeMap<K, V>>);

impl<K, V> CpuBTreeMap<K, V>
where
    K: Clone,
    V: Clone,
{
    pub fn new(cpu: usize) -> Self {
        CpuBTreeMap(vec![BTreeMap::<K, V>::new(); cpu])
    }

    pub fn contains_key(&self, cpu: usize, key: &K) -> bool
    where
        K: Ord,
    {
        self.0[cpu].contains_key(key)
    }

    pub fn insert(&mut self, cpu: usize, key: K, value: V)
    where
        K: Ord,
    {
        self.0[cpu].insert(key, value);
    }

    pub fn range(&mut self, cpu: usize, left: K, right: K) -> Vec<(&K, &V)>
    where
        K: Ord,
    {
        let l = Excluded(left);
        let r = Excluded(right);
        let mut res = vec![];
        for elem in self.0[cpu].range((l, r)) {
            res.push(elem);
        }
        res
    }

    pub fn lower_bound(&self, cpu: usize, val: K) -> Option<(&K, &V)>
    where
        K: Ord,
    {
        let mut res = self.0[cpu].range((Unbounded, Excluded(val)));
        res.next_back()
    }

    pub fn upper_bound(&self, cpu: usize, val: K) -> Option<(&K, &V)>
    where
        K: Ord,
    {
        let mut res = self.0[cpu].range((Excluded(val), Unbounded));
        res.next()
    }

    /// Delete old data
    pub fn flush(&mut self, cpu: usize, val: K)
    where
        K: Ord,
    {
        let new = self.0[cpu].split_off(&val);
        self.0[cpu] = new;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn lower_upper_bound() {
        let mut tree = CpuBTreeSet::new(1);
        tree.insert(0, 0);
        tree.insert(0, 1);
        tree.insert(0, 2);
        tree.insert(0, 3);
        tree.insert(0, 4);

        assert_eq!(tree.lower_bound(0, 0), None);
        assert_eq!(tree.lower_bound(0, 1), Some(&0));

        assert_eq!(tree.upper_bound(0, 3), Some(&4));
        assert_eq!(tree.upper_bound(0, 4), None);
    }

    #[test]
    fn flush() {
        let mut tree = CpuBTreeSet::new(1);
        tree.insert(0, 0);
        tree.insert(0, 1);
        tree.insert(0, 2);
        tree.insert(0, 3);
        tree.insert(0, 4);

        tree.flush(0, 3);
        assert_eq!(tree.0[0].first(), Some(&3));
        assert_eq!(tree.0[0].last(), Some(&4));
    }
}
