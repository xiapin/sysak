#include <argp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include "ioctl_cnt.h"
#include "./bpf/ioctl_cnt.skel.h"

struct env {
	time_t duration;
	bool verbose;
	__u64 threshold;
	__u32 tid;
	__u32 sysnr[128];
	struct filter filter;
} env = {
	.tid = -1,
	.duration = 0,
	.threshold = 10*1000*1000,	/* 10ms */
};

static int stackmp_fd;
volatile sig_atomic_t exiting = 0;


const char *argp_program_version = "ioctl_cnt 0.1";
const char argp_program_doc[] =
"Catch the delay of a syscall more than threshold.\n"
"\n"
"USAGE: ioctl_cnt [--help] [-t THRESH(ms)] [-n sys_NR] <[-c COMM] [-p tid]> [-f LOGFILE] [duration(s)]\n"
"\n"
"EXAMPLES:\n"
"    ioctl_cnt            # run forever, detect delay more than 10ms(default)\n"
"    ioctl_cnt -t 15      # detect with threshold 15ms (default 10ms)\n"
"    ioctl_cnt -p 1100    # detect tid 1100 (use for threads only)\n"
"    ioctl_cnt -c bash    # trace aplication who's name is bash\n"
"    ioctl_cnt -n 101,102 # Exclude syscall 123 from tracing.\n"
"    ioctl_cnt -f a.log   # log to a.log (default to ~sysak/ioctl_cnt.log)\n";

static const struct argp_option opts[] = {
	{ "threshold", 't', "THRESH", 0, "Threshold to detect, default 10ms"},
	{ "comm", 'c', "COMM", 0, "Name of the application"},
	{ "pid", 'p', "TID", 0, "Thread TID to detect"},
	{ "sysnr", 'n', "SYSNR", 0, "Exclude the syscall ID from tracing"},
	{ "logfile", 'f', "LOGFILE", 0, "logfile for result"},
	{ "verbose", 'v', NULL, 0, "Verbose debug output" },
	{ NULL, 'h', NULL, OPTION_HIDDEN, "Show the full help" },
	{},
};

static error_t parse_arg(int key, char *arg, struct argp_state *state)
{
	__u32 sysnr, pid;
	int i = 0, ret = errno;
	static int pos_args;
	char *tmp, *endptr;

	switch (key) {
	case 'h':
		argp_state_help(state, stderr, ARGP_HELP_STD_HELP);
		break;
	case 'v':
		env.verbose = true;
		break;
	case 't':
		errno = 0;
		__u64 thresh;
		thresh = strtoull(arg, NULL, 10);
		if (errno) {
			fprintf(stderr, "invalid threshold\n");
			argp_usage(state);
			break;
		}
		env.threshold = thresh * 1000*1000;
		break;
	case 'c':
		env.filter.size = strlen(arg);
		if (env.filter.size < 1) {
			fprintf(stderr, "Invalid COMM: %s\n", arg);
			argp_usage(state);
			return -1;
		}

		if (env.filter.size > TASK_COMM_LEN - 1)
			env.filter.size = TASK_COMM_LEN - 1;

		strncpy(env.filter.comm, arg, env.filter.size);
		break;
	case 'n':
		errno = 0;
		tmp = arg;
		do {
			sysnr = strtoul(tmp, &endptr, 10);
			ret = errno;
			if ((ret == ERANGE && (sysnr == LONG_MAX || sysnr == LONG_MIN))
				|| (ret != 0 && sysnr == 0)) {
				continue;
			}
			tmp = endptr+1;
			env.sysnr[i++] = sysnr;
			printf("i=%d, sysnr=%d\n", i-1, sysnr);
			if (*endptr != ',')
				break;
		} while(sysnr && i < 128);

		if (i == -1) {
			fprintf(stderr, "Invalid syscall num: %s\n", arg);
			argp_usage(state);
		}
		break;
	case 'p':
		errno = 0;
		pid = strtol(arg, NULL, 10);
		if (errno || pid <= 0) {
			fprintf(stderr, "Invalid TID: %s\n", arg);
			argp_usage(state);
		}
		env.tid = pid;
		break;
	case 'f':
		if (strlen(arg) < 2)
			strncpy(filename, defaultfile, sizeof(filename));
		else
			strncpy(filename, arg, sizeof(filename));
		filep = fopen(filename, "w+");
		if (!filep) {
			ret = errno;
			fprintf(stderr, "%s :fopen %s\n",
				strerror(errno), filename);
			return ret;
		}
		break;
	case ARGP_KEY_ARG:
		if (pos_args++) {
			fprintf(stderr,
				"unrecognized positional argument: %s\n", arg);
			argp_usage(state);
		}
		errno = 0;
		env.duration = strtol(arg, NULL, 10);
		if (errno) {
			ret = errno;
			fprintf(stderr, "invalid duration\n");
			argp_usage(state);
			return ret;
		}
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	if (!filep) {
		filep = fopen(defaultfile, "w+");
		if (!filep) {
			ret = errno;
			fprintf(stderr, "%s :fopen %s\n",
				strerror(errno), defaultfile);
			return ret;
		}
	}

	return 0;
}

static void bump_memlock_rlimit(void)
{
	struct rlimit rlim_new = {
		.rlim_cur = RLIM_INFINITY,
		.rlim_max = RLIM_INFINITY,
	};

	if (setrlimit(RLIMIT_MEMLOCK, &rlim_new)) {
		fprintf(stderr, "Failed to increase RLIMIT_MEMLOCK limit!\n");
		exit(1);
	}
}

void handle_event(void *ctx, int cpu, void *data, __u32 data_sz)
{
	const struct event *e = data;
	struct tm *tm;
	char ts[64];
	time_t t;

	time(&t);
	tm = localtime(&t);
	strftime(ts, sizeof(ts), "%F_%H:%M:%S", tm);
	if (0) {
		;
	} else {
		char *syscall, tmp[32];
		if ((e->sysid < MAX_NR) && (sys_array[e->sysid])) {
			syscall = sys_array[e->sysid];
		} else {
			snprintf(tmp, sizeof(tmp), "%ld", e->sysid);
			syscall = tmp;
		}
		fprintf(filep, "%-21s %-8lld %-6lld %-6lld %-6lld %-6lld %-6lld %-6lld %-9s %u(%s)\n",
			ts, e->delay/(1000*1000), e->realtime/(1000*1000),
			e->itime/(1000*1000), e->vtime/(1000*1000),
			e->stime/(1000*1000), e->nvcsw, e->nivcsw, syscall, e->pid, e->comm);
		fflush(filep);
		print_stack(stackmp_fd, e->ret, ksyms);
	}
}

void ioctl_cnt_handler(int cnt_fd, int map_fd, struct ioctl_cnt_bpf *obj)
{
	__u64 sum, cnt[128];
	int key = 0;

	memset(cnt, 0, sizeof(cnt))
	if (bpf_map_lookup_elem(cnt_fd, &next_key, cnt)) {
		printf("lookup_elem fail\n");
		return 0;
	}
	for (i = 0; i < 128; i++)
		sum += cnt[i]
	printf("sum=%llu\n", sum);
}

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
	if (level == LIBBPF_DEBUG && !env.verbose)
		return 0;
	return vfprintf(stderr, format, args);
}

static int prepare_directory(char *path)
{
	int ret;

	ret = mkdir(path, 0777);
	if (ret < 0 && errno != EEXIST)
		return errno;
	else
		return 0;
}

static void sig_exit(int signo)
{
	exiting = 1;
}

int main(int argc, char **argv)
{
	int err, ent_fd, arg_fd;
	struct ioctl_cnt_bpf *obj;
	static const struct argp argp = {
		.options = opts,
		.parser = parse_arg,
		.doc = argp_program_doc,
	};

	memset(env.sysnr, -1, sizeof(env.sysnr));
	memset(&env.filter, 0, sizeof(env.filter));
	err = argp_parse(&argp, argc, argv, 0, NULL, NULL);
	if (err) {
		fprintf(stderr, "argp_parse fail\n");
		return err;
	}
	libbpf_set_print(libbpf_print_fn);

	bump_memlock_rlimit();

	obj = ioctl_cnt_bpf__open_and_load();
	if (!obj) {
		fprintf(stderr, "failed to open and/or load BPF object\n");
		goto cleanup;
	}

	cnt_fd = bpf_map__fd(obj->maps.cnt_map);

	if (signal(SIGINT, sig_exit) == SIG_ERR ||
		signal(SIGALRM, sig_exit) == SIG_ERR) {
		fprintf(stderr, "can't set signal handler: %s\n", strerror(errno));
		err = 1;
		goto cleanup;
	}

	if (env.duration)
		alarm(env.duration);
	while (!exiting) {
		sleep(1);
		ioctl_cnt_handler(cnt_fd, 0, obj);
	}
cleanup:
	ioctl_cnt_bpf__destroy(obj);

	return err != 0;
}

