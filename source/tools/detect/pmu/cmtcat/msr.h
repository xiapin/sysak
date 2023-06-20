#define IA32_PQR_ASSOC (0xc8f)
#define IA32_QM_EVTSEL (0xc8d)
#define IA32_QM_CTR (0xc8e)
#define BUF_SIZE	1024

extern long nr_cpu;
typedef struct msr {
	int fd;
	__u32 rmid;
	long cpuid;
} msr_t;

int open_msr(int cpu_id)
{
	char path[BUF_SIZE];

	snprintf(path, BUF_SIZE, "/dev/cpu/%d/msr", cpu_id);
	int fd = open(path, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "Failed open %s.\n", path);
	}
	return fd;
}

/*
 *return value: <0 for fail, or the number of msr
 * */
int init_msr(msr_t **msrs)
{
	int i, err, suce;
	msr_t *p;

	p = calloc(nr_cpu, sizeof(msr_t));
	if (!p)
		return -errno;

	suce = -1;
	for (i = 0; i <  nr_cpu; i++) {
		p[i].cpuid = i;
		p[i].fd = open_msr(i);
		if (p[i].fd > 0)
			suce++;
	}
	*msrs = p;
	if (suce >= 0)
		return nr_cpu;
	else
		return -1;
}

int deinit_msr(msr_t *msrs, int nr)
{
	int i;

	for (i = 0; i < nr; i++) {
		if (msrs[i].fd > 0)
			close(msrs[i].fd);
	}
	return 0;
}

__u64 set_msr_assoc(msr_t* m, int rmid)
{
	__u64 msr_pqr_assoc = 0, msr_qm_evtsel = 0;
	__u64 val;

	pread(m->fd, &msr_pqr_assoc, sizeof(msr_pqr_assoc), IA32_PQR_ASSOC);
	msr_pqr_assoc &= 0xffffffff00000000ULL;
	msr_pqr_assoc |= (__u64)(rmid & ((1ULL<<10)-1ULL));
	pwrite(m->fd, &msr_pqr_assoc, sizeof(msr_pqr_assoc), IA32_PQR_ASSOC);

	msr_qm_evtsel = (__u64)(rmid & ((1ULL<<10)-1ULL));
	msr_qm_evtsel <<= 32;
	pwrite(m->fd, &msr_qm_evtsel, sizeof(msr_qm_evtsel), IA32_QM_EVTSEL);
}

/*
 * event=
 *  1 : L3 Oc
 *  2 : L3 Total External Bandwidth
 *  3 : L3 Local External Bandwidth
 *
 */
__u64 get_msr_count(msr_t* m, __u64 event)
{
	__u64 msr_qm_evtsel = 0, value = 0;
	__u64 val;

	pread(m->fd, &msr_qm_evtsel, sizeof(msr_qm_evtsel), IA32_QM_EVTSEL);
	msr_qm_evtsel &= 0xfffffffffffffff0ULL;
	msr_qm_evtsel |= event & ((1ULL << 8) - 1ULL);

	pwrite(m->fd, &msr_qm_evtsel, sizeof(msr_qm_evtsel), IA32_QM_EVTSEL);
	pread(m->fd, &val, sizeof(val), IA32_QM_CTR);
	return val;
}

__u64 read_l3_cache(msr_t *m)
{
	return get_msr_count(m, 1);
}

__u64 read_mb_total(msr_t *m)
{
	return get_msr_count(m, 2);
}

__u64 read_mb_local(msr_t *m)
{
	return get_msr_count(m, 3);
}
