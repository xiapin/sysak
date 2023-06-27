int get_cpus(long *nr_cpus)
{
	errno = 0;
	*nr_cpus = sysconf(_SC_NPROCESSORS_CONF);
	return errno;
}

static __u64 build_bit(__u32 beg, __u32 end)
{
	__u64 myll = 0;
	if (end == 63) {
		myll = -1;
	} else {
		myll = (1LL << (end + 1)) - 1;
	}
	myll = myll >> beg;
	return myll;
}

static __u64 extract_bits(__u64 myin, __u32 beg, __u32 end)
{
	__u64 myll = 0;
	__u32 beg1, end1;
	if (beg <= end) {
		beg1 = beg;
		end1 = end;
	} else {
		beg1 = end;
		end1 = beg;
	}
	myll = myin >> beg1;
	myll = myll & build_bit(beg1, end1);
	return myll;
}

__u64 extract_val(__u64 val)
{
	if (val & (3ULL << 62))
		return LLONG_MAX;

	return extract_bits(val, 0, 61);
}
