#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>	/* bpf_obj_pin */
#include <getopt.h>
#include "pidComm.h"
#include "bpf/reclaimhung.skel.h"
#include "reclaimhung.h"

__u64 period = DEF_TIME;
bool verbose = false;

char *log_dir = "/var/log/sysak/reclaimhung";
char *reclaim_data = "/var/log/sysak/reclaimhung/reclaim.log";
char *compact_data = "/var/log/sysak/reclaimhung/compaction.log";
char *cgroup_reclaim_data = "/var/log/sysak/reclaimhung/cgroup_reclaim.log";

struct option longopts[] = {
    { "time", no_argument, NULL, 't' },
    { "help", no_argument, NULL, 'h' },
    { 0, 0, 0, 0},
};

static void usage(void)
{
	fprintf(stdout,
	        "Usage: sysak reclaimhung [options]\n"
            "Options:\n"
            "    --time/-t     specify the monitor period(s), default=5000s\n"
            "    --help/-h     help info\n");
	exit(EXIT_FAILURE);
}

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
	if (verbose)
		return vfprintf(stderr, format, args);
	else
		return 0;
}

static void bump_memlock_rlimit(void)
{
	struct rlimit rlim_new = {
		.rlim_cur	= RLIM_INFINITY,
		.rlim_max	= RLIM_INFINITY,
	};

	if (setrlimit(RLIMIT_MEMLOCK, &rlim_new)) {
		fprintf(stderr, "Failed to increase RLIMIT_MEMLOCK limit!\n");
		exit(1);
	}
}

static void sig_int(int signo) { }

void stamp_to_date(__u64 stamp, char dt[], int len)
{
	time_t t, diff, last;
	struct tm *tm;
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	last = time(&t);
	if (stamp) {
		diff = ts.tv_sec * SEC_TO_NS + ts.tv_nsec - stamp;
		diff = diff / SEC_TO_NS;
		last = t - diff;
	}
	tm = localtime(&last);
	strftime(dt, len, "%F %H:%M:%S", tm);
}


static int prepare_directory(char *path)
{
	int ret = 0;

	if (access(path,0) != 0)
        ret = mkdir(path, 0777);

	return ret;
}

int main(int argc, char *argv[])
{
    int opt;
	char date[128];
    int err,fd;
	struct reclaimhung_bpf *skel;
    struct reclaim_data elem_r,cg_elem_r;
    struct compact_data elem_c;
	__u32 lookup_key, next_key;
	FILE *fp_re, *fp_cm, *fp_cg;

    while ((opt = getopt_long(argc, argv, "t:hv", longopts, NULL)) != -1) {
        switch (opt) {
            case 't':
                if (optarg)
                    period = (int)strtoul(optarg, NULL, 10);
                break;
            case 'h':
                usage();
                break;
            case 'v':
                verbose = true;
                break;
            default:
                printf("must have parameter\n");
                usage();
                break;
        }
    }

    libbpf_set_print(libbpf_print_fn);
    bump_memlock_rlimit();

    skel = reclaimhung_bpf__open();
	if (!skel) {
		fprintf(stderr, "Failed to open BPF skeleton\n");
		return 1;
	}

	err = reclaimhung_bpf__load(skel);
	if (err) {
		fprintf(stderr, "Failed to load BPF skeleton, errno:%d\n",err);
		return 1;
	}
    if (signal(SIGINT, sig_int) == SIG_ERR) {
		fprintf(stderr, "can't set signal handler: %s\n", strerror(errno));
		err = 1;
		goto cleanup;
	}

    err = reclaimhung_bpf__attach(skel);
	if (err) {
		fprintf(stderr, "failed to attach BPF skeleton\n");
		goto cleanup;
	}

    sleep(period);
	prepare_directory(log_dir);

	/* direct reclaim trace */
    fd = bpf_map__fd(skel->maps.reclaim_map);
	printf("Reclaim:\n");
    printf("%-16s\t%-16s\t%-16s\t%-16s\t%-32s\t%-16s\n", "pid", "name", "nr_reclaimed", "last_delay(ns)", "last_time", "nr_pages");
	
	if (access(reclaim_data,0) != 0){
		fp_re = fopen(reclaim_data, "a+");
		fprintf(fp_re, "%-16s\t%-16s\t%-16s\t%-16s\t%-32s\t%-16s\n", "pid", "name", "nr_reclaimed", "last_delay(ns)", "last_time", "nr_pages");
	} else {
		fp_re = fopen(reclaim_data, "a+");
	}

	lookup_key = -1;
	while (!bpf_map_get_next_key(fd, &lookup_key, &next_key)) {
		err = bpf_map_lookup_elem(fd, &next_key, &elem_r);
		if (err < 0) {
			fprintf(stderr, "failed to lookup exec: %d\n", err);
			return -1;

		}
		lookup_key = next_key;

		stamp_to_date(elem_r.da.time,date,128);
		printf("%-16d\t%-16s\t%-16u\t%-16llu\t%s(%-16llu)\t%-16d\n", elem_r.da.pid, elem_r.da.comm, elem_r.nr_reclaimed, elem_r.da.ts_delay, date, elem_r.da.time, elem_r.nr_pages);
		fprintf(fp_re, "%-16d\t%-16s\t%-16d\t%-16llu\t%s(%-16llu)\t%-16d\n", elem_r.da.pid, elem_r.da.comm, elem_r.nr_reclaimed, elem_r.da.ts_delay, date, elem_r.da.time, elem_r.nr_pages);
	}
	fclose(fp_re);

	/* compact trace */
    fd = bpf_map__fd(skel->maps.compact_map);
	printf("Compaction:\n");
    printf("%-16s\t%-16s\t%-16s\t%-16s\t%-32s\t%-16s\n", "pid", "name", "nr_compacted", "last_delay(ns)", "last_time", "result");

	if (access(compact_data,0) != 0){
		fp_cm = fopen(compact_data, "a+");
		fprintf(fp_cm, "%-16s\t%-16s\t%-16s\t%-16s\t%-32s\t%-16s\n", "pid", "name", "nr_compacted", "last_delay(ns)", "last_time", "result");
	} else {
		fp_cm = fopen(compact_data, "a+");
	}

	lookup_key = -1;
	while (!bpf_map_get_next_key(fd, &lookup_key, &next_key)) {
		err = bpf_map_lookup_elem(fd, &next_key, &elem_c);
		if (err < 0) {
			fprintf(stderr, "failed to lookup exec: %d\n", err);
			return -1;

		}
		lookup_key = next_key;

		stamp_to_date(elem_c.da.time,date,128);
		printf("%-16d\t%-16s\t%-16u\t%-16llu\t%s(%-16llu)\t%-16d\n", elem_c.da.pid, elem_c.da.comm, elem_c.nr_compacted, elem_c.da.ts_delay, date, elem_c.da.time, elem_c.status);
		fprintf(fp_cm, "%-16d\t%-16s\t%-16u\t%-16llu\t%-16s(%llu)\t%-16d\n", elem_c.da.pid, elem_c.da.comm, elem_c.nr_compacted, elem_c.da.ts_delay, date, elem_c.da.time, elem_c.status);
	}
	fclose(fp_cm);

	/* cgroup reclaim trace */
    fd = bpf_map__fd(skel->maps.cgroup_map);
	printf("Cgroup Reclaim:\n");
    printf("%-16s\t%-16s\t%-16s\t%-16s\t%-32s\t%-16s\n", "pid", "name", "nr_reclaimed", "last_delay(ns)", "last_time", "nr_pages");
	

	if (access(cgroup_reclaim_data,0) != 0){
		fp_cg = fopen(cgroup_reclaim_data, "a+");
		fprintf(fp_cg, "%-16s\t%-16s\t%-16s\t%-16s\t%-32s\t%-16s\n", "pid", "name", "nr_reclaimed", "last_delay(ns)", "last_time", "nr_pages");
	} else {
		fp_cg = fopen(cgroup_reclaim_data, "a+");
	}

	lookup_key = -1;
	while (!bpf_map_get_next_key(fd, &lookup_key, &next_key)) {
		err = bpf_map_lookup_elem(fd, &next_key, &cg_elem_r);
		if (err < 0) {
			fprintf(stderr, "failed to lookup exec: %d\n", err);
			return -1;

		}
		lookup_key = next_key;

		stamp_to_date(cg_elem_r.da.time,date,128);
		printf("%-16d\t%-16s\t%-16u\t%-16llu\t%s(%-16llu)\t%-16d\n", cg_elem_r.da.pid, cg_elem_r.da.comm, cg_elem_r.nr_reclaimed, cg_elem_r.da.ts_delay, date, cg_elem_r.da.time, elem_r.nr_pages);
		fprintf(fp_cg, "%-16d\t%-16s\t%-16u\t%-16llu\t%s(%-16llu)\t%-16d\n", cg_elem_r.da.pid, cg_elem_r.da.comm, cg_elem_r.nr_reclaimed, cg_elem_r.da.ts_delay, date, cg_elem_r.da.time, elem_r.nr_pages);
	}
	fclose(fp_cg);

cleanup:
	reclaimhung_bpf__destroy(skel);
	return 0;
}