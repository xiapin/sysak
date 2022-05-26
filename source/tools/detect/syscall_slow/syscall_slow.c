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
#include "syscall_slow.h"
#include "./bpf/syscall_slow.skel.h"

struct env {
	time_t duration;
	bool verbose;
	__u64 threshold;
	__u32 tid, sysnr;
	struct filter filter;
} env = {
	.tid = -1,
	.sysnr = -1,
	.duration = 0,
	.threshold = 10*1000*1000,	/* 10ms */
};

FILE *filep = NULL;
static int stackmp_fd;
static struct ksym *ksyms;
char filename[256] = {0};
char log_dir[] = "/var/log/sysak/syscall_slow";
char defaultfile[] = "/var/log/sysak/syscall_slow/syscall_slow.log";
volatile sig_atomic_t exiting = 0;

void print_stack(int fd, __u32 ret, struct ksym *syms);
int load_kallsyms(struct ksym **pksyms);
const char *argp_program_version = "syscall_slow 0.1";
const char argp_program_doc[] =
"Catch the delay of a syscall more than threshold.\n"
"\n"
"USAGE: syscall_slow [--help] [-t THRESH(ms)] [-n sys_NR] <[-c COMM] [-p tid]> [-f LOGFILE] [duration(s)]\n"
"\n"
"EXAMPLES:\n"
"    syscall_slow            # run forever, detect delay more than 10ms(default)\n"
"    syscall_slow -t 15      # detect with threshold 15ms (default 10ms)\n"
"    syscall_slow -p 1100    # detect tid 1100 (use for threads only)\n"
"    syscall_slow -c bash    # trace aplication who's name is bash\n"
"    syscall_slow -n sysnr   # trace syscall sysnr\n"
"    syscall_slow -f a.log   # log to a.log (default to ~sysak/syscall_slow.log)\n";

static const struct argp_option opts[] = {
	{ "threshold", 't', "THRESH", 0, "Threshold to detect, default 10ms"},
	{ "comm", 'c', "COMM", 0, "Name of the application"},
	{ "pid", 'p', "TID", 0, "Thread TID to detect"},
	{ "sysnr", 'n', "SYSNR", 0, "Syscall ID to detect"},
	{ "logfile", 'f', "LOGFILE", 0, "logfile for result"},
	{ "verbose", 'v', NULL, 0, "Verbose debug output" },
	{ NULL, 'h', NULL, OPTION_HIDDEN, "Show the full help" },
	{},
};

static error_t parse_arg(int key, char *arg, struct argp_state *state)
{
	__u32 sysnr, pid;
	int ret = errno;
	static int pos_args;

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
		sysnr = strtol(arg, NULL, 10);
		if (errno || sysnr < 0) {
			fprintf(stderr, "Invalid syscall num: %s\n", arg);
			argp_usage(state);
		}
		env.sysnr = sysnr;
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
		fprintf(filep, "%-21s %-8lld %-6lld %-6lld %-6lld %-6lld %-6lld %-6lld %-6ld\n",
			ts, e->delay/(1000*1000), e->realtime/(1000*1000),
			e->itime/(1000*1000), e->vtime/(1000*1000),
			e->stime/(1000*1000), e->nvcsw, e->nivcsw, e->sysid);
		fflush(filep);
		print_stack(stackmp_fd, e->ret, ksyms);
	}
}

void syscall_slow_handler(int poll_fd, int map_fd)
{
	int arg_key = 0, err = 0;
	struct arg_info arg_info = {};
	struct perf_buffer *pb = NULL;
	struct perf_buffer_opts pb_opts = {};

	fprintf(filep, "%-21s %-8s %-6s %-6s %-6s %-6s %-6s %-6s %-6s\n",
		"TIME(syscall)", "DELAY", "REAL", "WAIT", "SLEEP",
		"SYS", "vcsw", "ivcsw", "sysid");

	pb_opts.sample_cb = handle_event;
	pb = perf_buffer__new(poll_fd, 64, &pb_opts);
	if (!pb) {
		err = -errno;
		fprintf(stderr, "failed to open perf buffer: %d\n", err);
		goto clean_syscall_slow;
	}

	arg_info.thresh = env.threshold;
	arg_info.filter = env.filter;
	arg_info.pid = env.tid;
	arg_info.sysnr = env.sysnr;
	err = bpf_map_update_elem(map_fd, &arg_key, &arg_info, 0);
	if (err) {
		fprintf(stderr, "Failed to update arg_map\n");
		goto clean_syscall_slow;
	}

	while (!exiting) {
		err = perf_buffer__poll(pb, 100);
		if (err < 0 && err != -EINTR) {
			fprintf(stderr, "error polling perf buffer: %s\n", strerror(-err));
			goto clean_syscall_slow;
		}
		/* reset err to return 0 if exiting */
		err = 0;
	}

clean_syscall_slow:
	perf_buffer__free(pb);
}

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
	if (level == LIBBPF_DEBUG && !env.verbose)
		return 0;
	return vfprintf(stderr, format, args);
}

static int prepare_dictory(char *path)
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
	struct syscall_slow_bpf *obj;
	static const struct argp argp = {
		.options = opts,
		.parser = parse_arg,
		.doc = argp_program_doc,
	};

	err = prepare_dictory(log_dir);
	if (err) {
		fprintf(stderr, "prepare_dictory %s fail\n", log_dir);
		return err;
	}
	ksyms = NULL;

	memset(&env.filter, 0, sizeof(struct filter));
	err = argp_parse(&argp, argc, argv, 0, NULL, NULL);
	if (err) {
		fprintf(stderr, "argp_parse fail\n");
		return err;
	}
	libbpf_set_print(libbpf_print_fn);

	bump_memlock_rlimit();
	err = load_kallsyms(&ksyms);
	if (err) {
		fprintf(stderr, "Failed to load kallsyms\n");
		return err;
	}

	obj = syscall_slow_bpf__open_and_load();
	if (!obj) {
		fprintf(stderr, "failed to open and/or load BPF object\n");
		goto cleanup;
	}

	err = syscall_slow_bpf__attach(obj);
	if (err) {
		fprintf(stderr, "failed to attach BPF programs\n");
		goto cleanup;
	}

	arg_fd = bpf_map__fd(obj->maps.arg_map);
	ent_fd = bpf_map__fd(obj->maps.events);
	stackmp_fd = bpf_map__fd(obj->maps.stackmap);

	if (signal(SIGINT, sig_exit) == SIG_ERR ||
		signal(SIGALRM, sig_exit) == SIG_ERR) {
		fprintf(stderr, "can't set signal handler: %s\n", strerror(errno));
		err = 1;
		goto cleanup;
	}

	if (env.duration)
		alarm(env.duration);

	syscall_slow_handler(ent_fd, arg_fd);

cleanup:
	syscall_slow_bpf__destroy(obj);
	if (ksyms)
		free(ksyms);

	return err != 0;
}

