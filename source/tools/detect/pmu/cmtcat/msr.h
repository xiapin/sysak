
int msr_write(int fd, __u64 msr_number, __u64 value)
{
#if 0
	static std::mutex m;
	std::lock_guard<std::mutex> g(m);
	std::cout << "DEBUG: writing MSR 0x" << std::hex << msr_number << " value 0x" << value << " on cpu " << std::dec << cpu_id << std::endl;
#endif
	if (fd < 0)
		return 0;
	return pwrite(fd, (const void *)&value, sizeof(__u64), msr_number);
}

int msr_read(int fd, __u64 msr_number, __u64 * value)
{
	if (fd < 0)
		return 0;
	return pread(fd, (void *)value, sizeof(__u64), msr_number);
}
