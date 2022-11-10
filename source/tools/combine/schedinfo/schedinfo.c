#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>	/* bpf_obj_pin */
#include <getopt.h>
#include "pidComm.h"
#include "sched_jit.h"
#include "bpf/schedinfo.skel.h"
#include "schedinfo.h"

unsigned int nr_cpus;
static volatile sig_atomic_t exiting;
char *help_str = "sysak schedinfo";
struct environment {
	int span;
	bool verbose;
} env = {
	.span = 0,
	.verbose = false,
};

int parse_proc_stat(struct schedinfo *schedinfo);
int record_top_util_proces(struct top_utils *top);
int init_top_struct(struct top_utils *top, char *prefix);

static void usage(char *prog)
{
	const char *str =
	"  Usage: %s [OPTIONS]\n"
	"  Options:\n"
	"  -t THRESH_TIME       specify the threshold time(ms), default=10ms\n"
	"  -f result.log        result file, default is /var/log/sysak/schedinfo.log\n"
	"  -s TIME              specify how long to run \n"
	"  -S shmkey            record the result as summary mod\n"
	;

	fprintf(stderr, str, prog);
	exit(EXIT_FAILURE);
}

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
	if (env.verbose)
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

int schedinfo_collect(struct schedinfo *schedinfo)
{
	int ret;

	ret = parse_proc_stat(schedinfo);
	//parse_proc_schedstat(schedinfo);
	//parse_loadavg(schedinfo);
	return ret;
}

void get_date(char *ts)
{
	time_t t;
	struct tm *tm;

	time(&t);
	tm = localtime(&t);
	strftime(ts, sizeof(ts), "%F %H:%M:%S", tm);
}

int schedinfo_handler(int fd, struct schedinfo_bpf *skel, struct schedinfo *schedinfo)
{
	int err = 0;
	struct rq_info rqinfo;

	/* Attach tracepoint handler */
	err = schedinfo_bpf__attach(skel);
	if (err) {
		fprintf(stderr, "Failed to attach BPF skeleton\n");
		return err;
	}

	while (!exiting) {
		unsigned long delta_fork, delta_io;
		unsigned long prv_fork, prv_io, nr_unint = 0;
		__u32 key = -1;

		schedinfo_collect(schedinfo);
		prv_fork = schedinfo->nr_forked;
		prv_io = schedinfo->nr_block;
		sleep(2);
		schedinfo_collect(schedinfo);
		delta_fork = schedinfo->nr_forked - prv_fork;
		delta_io = schedinfo->nr_block - prv_io;
		/* This is for array maps, hash maps choose bpf_map_get_next_key()  */
		for (key = 0; key < nr_cpus; key++) {
			if (bpf_map_lookup_elem(fd, &key, &rqinfo) == 0) {
				nr_unint += rqinfo.nr_uninterruptible;
				schedinfo->datass[key].nr_running = rqinfo.nr_running;
				printf("cpu%d: nr_running=%u, user=%5.2f, sys=%5.2f, idle=%5.2f\n",
					rqinfo.cpu, rqinfo.nr_running,
					100*schedinfo->datass[key].user_util,
					100*schedinfo->datass[key].sys_util,
					100*schedinfo->datass[key].idle_util);
			} else {
				printf("bpf_map_loockup_elem fail\n");
			}
		}
		schedinfo->nr_uninterruptible = nr_unint;
		printf("total: nr_runing=%llu, user=%5.2f, sys=%5.2f, forked=%lu, io=%lu, uni=%lu\n",
					schedinfo->allcpu->nr_running,
					100*schedinfo->allcpu->user_util,
					100*schedinfo->allcpu->sys_util,
					delta_fork, delta_io, schedinfo->nr_uninterruptible);

		record_top_util_proces(&schedinfo->top);
	}

	return err;
}

static void sig_exit(int signo)
{
	exiting = 1;
}

int parse_args(int argc, char **argv, struct environment *env)
{
	int ret, c, option_index;
	unsigned long span = 0;

	ret = -EINVAL;
	for (;;) {
		c = getopt_long(argc, argv, "s:vh", NULL, &option_index);
		if (c == -1)
			break;
		switch (c) {
			case 's':
				span = (int)strtoul(optarg, NULL, 10);
				if ((errno == ERANGE && (span == LONG_MAX || span == LONG_MIN))
					|| (errno != 0 && span == 0)) {
					perror("strtoul");
					ret = errno;
					goto parse_out;
				}
				ret = 0;
				env->span = span;
				break;
			case 'v':
				ret = 0;
				env->verbose = true;
				break;
			case 'h':
				usage(help_str);	/* would exit */
				break;
			default:
				usage(help_str);
		}
	}

parse_out:
	return ret;
}

int schedinfo_init(struct schedinfo *schedinfo)
{
	int ret = 0;
	size_t sd_size;
	struct sched_datas *datass, *prev, *allcpu, *allcpuprev;

	nr_cpus = libbpf_num_possible_cpus();
	if (nr_cpus < 0) {
		fprintf(stderr, "failed to get # of possible cpus: '%s'!\n",
			strerror(-nr_cpus));
		return -nr_cpus;
	}

	sd_size = sizeof(struct sched_datas);
	prev = calloc(nr_cpus, sd_size);
	if (!prev) {
		ret = errno;
		goto out_ini;
	}
	datass = calloc(nr_cpus, sd_size);
	if (!datass) {
		ret = errno;
		free(prev);
		goto out_ini;
	}

	allcpu = malloc(sd_size);
	if (!allcpu) {
		free(prev);
		free(datass);
		ret = errno;
		goto out_ini;
	}

	allcpuprev = malloc(sd_size);
	if (!allcpuprev) {
		free(prev);
		free(datass);
		free(allcpu);
		ret = errno;
		goto out_ini;
	}
	memset(prev, 0, nr_cpus*sd_size);
	memset(datass, 0, nr_cpus*sd_size);
	memset(allcpu, 0, sd_size);
	memset(allcpuprev, 0, sd_size);
	schedinfo->prev = prev;
	schedinfo->datass = datass;
	schedinfo->allcpu = allcpu;
	schedinfo->allcpuprev = allcpuprev;

	ret = init_top_struct(&schedinfo->top, SYSAK_LOG_PATH);
out_ini:
	return ret;
}

int main(int argc, char **argv)
{
	struct schedinfo_bpf *skel;
	int err, map_fd;
	struct schedinfo schedinfo = {0};

	parse_args(argc, argv, &env);

	libbpf_set_print(libbpf_print_fn);
	bump_memlock_rlimit();

	err = schedinfo_init(&schedinfo);
	if (err) {
		fprintf(stderr, "Failed to schedinfo_init, ret=%d\n", err);
		return 1;
	}

	/* Open load and verify BPF application */
	skel = schedinfo_bpf__open();
	if (!skel) {
		fprintf(stderr, "Failed to open BPF skeleton\n");
		return 1;
	}

	err = schedinfo_bpf__load(skel);
	if (err) {
		fprintf(stderr, "Failed to load BPF skeleton\n");
		return 1;
	}

	map_fd = bpf_map__fd(skel->maps.rq_map);
	if (signal(SIGINT, sig_exit) == SIG_ERR ||
		signal(SIGALRM, sig_exit) == SIG_ERR || 
		signal(SIGTERM, sig_exit) == SIG_ERR) {
		fprintf(stderr, "can't set signal handler: %s\n", strerror(errno));
		goto cleanup;
	}

	if (env.span)
		alarm(env.span);

	err = schedinfo_handler(map_fd, skel, &schedinfo);

cleanup:
	return -err;
}
