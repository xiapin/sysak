
int get_cpus(long *nr_cpus)
{
	errno = 0;
	*nr_cpus = sysconf(_SC_NPROCESSORS_CONF);
	return errno;
}
