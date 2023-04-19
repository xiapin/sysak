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
	__u32 tid;
	__u32 sysnr[128];
	struct filter filter;
} env = {
	.tid = -1,
	.duration = 0,
	.threshold = 10*1000*1000,	/* 10ms */
};

FILE *filep = NULL;
static int stackmp_fd;
static struct ksym *ksyms;
char filename[256] = {0};
char *sys_array[MAX_NR];
char log_dir[] = "/var/log/sysak/syscall_slow";
char defaultfile[] = "/var/log/sysak/syscall_slow/syscall_slow.log";
volatile sig_atomic_t exiting = 0;

void print_stack(int fd, __u32 ret, struct ksym *syms);
int load_kallsyms(struct ksym **pksyms);
int nr_to_syscall(int argc, char *arry[]);

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
"    syscall_slow -n 101,102 # Exclude syscall 101 and 102 from tracing.\n"
"    syscall_slow -f a.log   # log to a.log (default to ~sysak/syscall_slow.log)\n";

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
		unsigned long val = strtoul(arg, &endptr, 10);

        if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) ||
            (errno != 0 && val == 0)) {
            ret = errno;
            fprintf(stderr, "invalid duration\n");
            argp_usage(state);
            return ret;
        }

        if (endptr == arg) {
            ret = EINVAL;
            fprintf(stderr, "invalid duration\n");
            argp_usage(state);
            return ret;
        }

        env.duration = val;
        break;
	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

static int open_log_file(void) {
    int ret = 0;
    if (!filep) {
        filep = fopen(defaultfile, "w+");
        if (!filep) {
            ret = errno;
            fprintf(stderr, "%s :fopen %s\n", strerror(errno), defaultfile);
            return ret;
        }
    }
    return ret;
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

void syscall_slow_handler(int poll_fd, int map_fd, struct syscall_slow_bpf *obj)
{
	int i = 0, sysnr_fd;
	int arg_key = 0, err = 0;
	struct arg_info arg_info = {};
	struct perf_buffer *pb = NULL;
	struct perf_buffer_opts pb_opts = {};

	fprintf(filep, "%-21s %-8s %-6s %-6s %-6s %-6s %-6s %-6s %-9s %s\n",
		"TIME(syscall)", "DELAY", "REAL", "WAIT", "SLEEP",
		"SYS", "vcsw", "ivcsw", "syscall", "pid(comm)");

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
	err = bpf_map_update_elem(map_fd, &arg_key, &arg_info, 0);
	if (err) {
		fprintf(stderr, "Failed to update arg_map\n");
		goto clean_syscall_slow;
	}

	sysnr_fd = bpf_map__fd(obj->maps.sysnr_map);
	while (i < 128) {
		err = bpf_map_update_elem(sysnr_fd, &env.sysnr[i], &env.sysnr[i], 0);
		if (err) {
			fprintf(stderr, "Failed to update arg_map\n");
			goto clean_syscall_slow;
		}
		i++;
	}

	err = syscall_slow_bpf__attach(obj);
	if (err) {
		fprintf(stderr, "failed to attach BPF programs\n");
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
	struct syscall_slow_bpf *obj;
	static const struct argp argp = {
		.options = opts,
		.parser = parse_arg,
		.doc = argp_program_doc,
	};

	err = prepare_directory(log_dir);
	if (err) {
		fprintf(stderr, "prepare_directory %s fail\n", log_dir);
		return err;
	}
	ksyms = NULL;

	memset(env.sysnr, -1, sizeof(env.sysnr));
	memset(&env.filter, 0, sizeof(env.filter));
	err = argp_parse(&argp, argc, argv, 0, NULL, NULL);
	if (err) {
		fprintf(stderr, "argp_parse fail\n");
		return err;
	}

    err = open_log_file();
    if (err) {
        fprintf(stderr, "Failed to open log file\n");
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

	memset(sys_array, 0, sizeof(sys_array));
	err = nr_to_syscall(0, sys_array);
	syscall_slow_handler(ent_fd, arg_fd, obj);

cleanup:
	syscall_slow_bpf__destroy(obj);
	if (ksyms)
		free(ksyms);

    if (filep) 
		fclose(filep);
    return err != 0;
}

