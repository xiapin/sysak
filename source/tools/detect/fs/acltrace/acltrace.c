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
#include "bpf/acltrace.skel.h"
#include "acltrace.h"

__u64 period = DEF_TIME;
bool verbose = false;

char *log_dir = "/var/log/sysak/acltrace";
char *log_data = "/var/log/sysak/acltrace/acltrace.log";

struct option longopts[] = {
    { "time", no_argument, NULL, 't' },
    { "help", no_argument, NULL, 'h' },
    { 0, 0, 0, 0},
};

static void usage(void)
{
	fprintf(stdout,
	        "Usage: sysak acltrace [options]\n"
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


static int prepare_dictory(char *path)
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
	struct acltrace_bpf *skel;
    struct acl_data elem;
	__u32 lookup_key, next_key;
	FILE *fp;

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

    skel = acltrace_bpf__open();
	if (!skel) {
		fprintf(stderr, "Failed to open BPF skeleton\n");
		return 1;
	}

	err = acltrace_bpf__load(skel);
	if (err) {
		fprintf(stderr, "Failed to load BPF skeleton, errno:%d\n",err);
		return 1;
	}
    if (signal(SIGINT, sig_int) == SIG_ERR) {
		fprintf(stderr, "can't set signal handler: %s\n", strerror(errno));
		err = 1;
		goto cleanup;
	}

    err = acltrace_bpf__attach(skel);
	if (err) {
		fprintf(stderr, "failed to attach BPF skeleton\n");
		goto cleanup;
	}
	printf("Starting trace, Can hit <Ctrl+C> to abort and report\n");
    sleep(period);
	prepare_dictory(log_dir);

	/* direct reclaim trace */
    fd = bpf_map__fd(skel->maps.acl_map);
	printf("Reclaim:\n");
    printf("%-12s\t%-12s\t%-12s\t%-16s\t%-12s\t%-16s\n", "pid", "comm", "dentry", "xattrs", "count", "last_time");
	
	if (access(log_data,0) != 0){
		fp = fopen(log_data, "a+");
		fprintf(fp, "%-12s\t%-12s\t%-12s\t%-16s\t%-12s\t%-16s\n", "pid", "comm", "dentry", "xattrs", "count","last_time");
	} else {
		fp = fopen(log_data, "a+");
	}

	while (!bpf_map_get_next_key(fd, &lookup_key, &next_key)) {
		err = bpf_map_lookup_elem(fd, &next_key, &elem);
		if (err < 0) {
			fprintf(stderr, "failed to lookup exec: %d\n", err);
			return -1;

		}
		lookup_key = next_key;

		stamp_to_date(elem.time,date,128);
		printf("%-12d\t%-12s\t%-12s\t%-16s\t%-12d\t%-16s\n", elem.pid, elem.comm, elem.dentryname, elem.xattrs, elem.count, date);
		fprintf(fp, "%-12d\t%-12s\t%-12s\t%-16s\t%-12d\t%-16s\n", elem.pid, elem.comm, elem.dentryname, elem.xattrs, elem.count, date);
	}
	fclose(fp);

cleanup:
	acltrace_bpf__destroy(skel);
	return 0;
}