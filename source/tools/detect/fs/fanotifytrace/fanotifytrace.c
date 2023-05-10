#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <getopt.h>
#include "bpf/fanotifytrace.skel.h"
#include "fanotifytrace.h"

long int syscallid = FANOTIFY_INIT_ID;
bool verbose = false;
FILE *fanotifyinit_fd = NULL;
static volatile sig_atomic_t exiting;


char *log_dir = "/var/log/sysak/fanotifytrace";
char *fanotifyinit_data = "/var/log/sysak/fanotifytrace/fanotify_init.log";

struct option longopts[] = {
    //{ "time", no_argument, NULL, 't' },
	{ "syscallid", no_argument, NULL, 'i' },
    { "help", no_argument, NULL, 'h' },
    { 0, 0, 0, 0},
};

static void usage(void)
{
	fprintf(stdout,
	        "Usage: sysak fanotifytrace [options] [args]\n"
            "Options:\n"
			"    --syscallid/-i		specify the syscallid,default is fanotify_init id\n"
            "    --help/-h			help info\n"
            "example: sysak fanotifytrace #trace fanotify_init\n");
	exit(EXIT_FAILURE);
}

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
	if (verbose)
		return vfprintf(stderr, format, args);
	else
		return 0;
}

static void sig_int(int signo)
{
	exiting = 1;
}

static void sig_alarm(int signo)
{
	exiting = 1;
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

void handle_event(void *ctx, int cpu, void *data, __u32 data_sz)
{
	const struct task_info *dp = data;
	char date[MAX_DATE];

	stamp_to_date(dp->time, date, MAX_DATE);
	fprintf(fanotifyinit_fd, "%-48s\t%-10u\t%-16s\t%-12s\t%-10lu\n", dp->comm, dp->pid, date, "fanotify_init", dp->syscallid);
}

void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt)
{
	printf("Lost %llu events on CPU #%d!\n", lost_cnt, cpu);
}

int fanotify_handler(int poll_fd, struct fanotifytrace_bpf *skel)
{
	int err = 0;
	struct perf_buffer *pb = NULL;
	struct perf_buffer_opts pb_opts = {};

	if (access(fanotifyinit_data,0) != 0){
		fanotifyinit_fd = fopen(fanotifyinit_data, "a+");
		fprintf(fanotifyinit_fd, "%-48s\t%-10s\t%-16s\t%-12s\t%-10s\n", "Name", "Pid","Time", "event", "Syscall Number");			
	} else {
		fanotifyinit_fd = fopen(fanotifyinit_data, "a+");
	}

	pb_opts.sample_cb = handle_event;
	pb_opts.lost_cb = handle_lost_events;
	pb = perf_buffer__new(poll_fd, 64, &pb_opts);
	if (!pb) {
		err = -errno;
		fprintf(stderr, "failed to open perf buffer: %d\n", err);
		goto clean_nosched;
	}

	err = fanotifytrace_bpf__attach(skel);
	if (err) {
		fprintf(stderr, "Failed to attach BPF skeleton\n");
		return err;
	}
	while (!exiting) {
		err = perf_buffer__poll(pb, 100);
		if (err < 0 && err != -EINTR) {
			fprintf(stderr, "error polling perf buffer: %s\n", strerror(-err));
			goto clean_nosched;
		}
		/* reset err to return 0 if exiting */
		err = 0;
	}
clean_nosched:
	fclose(fanotifyinit_fd);
	perf_buffer__free(pb);
	return err;
}

int main(int argc, char *argv[])
{
	int opt;
	int err = 0, map_fd1, map_fd2, args_key = 0;
	struct fanotifytrace_bpf *skel;
	struct args args;

	while ((opt = getopt_long(argc, argv, "i:hv", longopts, NULL)) != -1) {
		switch (opt) {
			case 'i':
                if (optarg)
                    syscallid = (long int)strtoul(optarg, NULL, 10);
				break;
			case 'h':
                usage();
                break;
			case 'v':
                verbose = true;
                break;
			default:
                break;
        }
	}

	libbpf_set_print(libbpf_print_fn);
	bump_memlock_rlimit();

	prepare_dictory(log_dir);

	skel = fanotifytrace_bpf__open();
	if (!skel) {
		fprintf(stderr, "Failed to open BPF skeleton\n");
		return 1;
	}

	err = fanotifytrace_bpf__load(skel);
	if (err) {
		fprintf(stderr, "Failed to load BPF skeleton, errno:%d\n",err);
		return 1;
	}
	if (signal(SIGINT, sig_int) == SIG_ERR ||
		signal(SIGALRM, sig_alarm) == SIG_ERR) {
		fprintf(stderr, "can't set signal handler: %s\n", strerror(errno));
		goto cleanup;
	}

	map_fd1 = bpf_map__fd(skel->maps.args_map);
	args.syscallid = syscallid;
	err = bpf_map_update_elem(map_fd1, &args_key, &args, 0);
	if (err) {
		fprintf(stderr, "Failed to update flag map\n");
		goto cleanup;
	}
	map_fd2 = bpf_map__fd(skel->maps.fanotify_events);

	printf("Starting trace, Can hit <Ctrl+C> to abort and report\n");
	err = fanotify_handler(map_fd2, skel);

cleanup:
	fanotifytrace_bpf__destroy(skel);
	return err;
}
