#include <argp.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include "kvmexittime.h"
#include "exit_reason.h"
#include "bpf/kvmexittime.skel.h"

struct env {
	pid_t pid;
	pid_t tid;
	time_t interval;
	bool verbose;
} env = {
	.interval = 1,
};

const char *argp_program_version = "kvmexittime 0.1";
const char argp_program_doc[] =
"Trace kvm exit time.\n"
"\n"
"USAGE: kvmexittime [--help] [-p PID] [-t TID] [interval]\n"
"\n"
"EXAMPLES:\n"
"    kvmexittime        # trace whole system kvm exit time\n"
"    kvmexittime 5      # trace 5 seconds summaries\n"
"    kvmexittime -p 123 # trace kvm exit time for pid 123\n"
"    kvmexittime -t 123 # trace kvm exit time for tid 123 (use for threads only)";

static const struct argp_option opts[] = {
	{ "pid", 'p', "PID", 0, "Process PID to trace"},
	{ "tid", 't', "TID", 0, "Thread ID to trace"},
	{ "verbose", 'v', NULL, 0, "Verbose debug output" },
	{ NULL, 'h', NULL, OPTION_HIDDEN, "Show the full help" },
	{},
};

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
	if (level == LIBBPF_DEBUG && !env.verbose)
		return 0;
	return vfprintf(stderr, format, args);
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

static void sig_int(int signo) {}

static error_t parse_arg(int key, char *arg, struct argp_state *state)
{
	int pid, tid;
	static int pos_args;

	switch (key) {
	case 'h':
		argp_state_help(state, stderr, ARGP_HELP_STD_HELP);
		break;
	case 'v':
		env.verbose = true;
		break;
	case 'p':
		errno = 0;
		pid = strtol(arg, NULL, 10);
		if (errno || pid <= 0) {
			fprintf(stderr, "Invalid PID: %s\n", arg);
			argp_usage(state);
		}
		env.pid = pid;
		break;
	case 't':
		errno = 0;
		tid = strtol(arg, NULL, 10);
		if (errno || tid <= 0) {
			fprintf(stderr, "Invalid TID: %s\n", arg);
			argp_usage(state);
		}
		env.tid = tid;
		break;
	case ARGP_KEY_ARG:
		errno = 0;
		if (pos_args == 0) {
			env.interval = strtol(arg, NULL, 10);
			if (errno) {
				fprintf(stderr, "invalid internal\n");
				argp_usage(state);
			}
		} else {
			fprintf(stderr,
				"unrecognized positional argument: %s\n", arg);
			argp_usage(state);
		}
		pos_args++;
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

int main(int argc, char **argv)
{
	int err, map_fd, fd;
	static const struct argp argp = {
		.options = opts,
		.parser = parse_arg,
		.doc = argp_program_doc,
	};
	struct kvmexittime_bpf *obj;
	struct args args = {};
 	struct kvm_exit_time tm;
 	__u32 lookup_key, next_key;
	__u64 i=0, total_cnt = 0, total_ct = 0, total_sot = 0, total_oct = 0;

	err = argp_parse(&argp, argc, argv, 0, NULL, NULL);
	if (err)
		return err;

	libbpf_set_print(libbpf_print_fn);
	
	bump_memlock_rlimit();
	
	obj = kvmexittime_bpf__open();
	if (!obj) {
		fprintf(stderr, "failed to open BPF object\n");
		return 1;
	}

	err = kvmexittime_bpf__load(obj);
	if (err) {
		fprintf(stderr, "failed to load BPF object: %d\n", err);
		goto cleanup;
	}

	map_fd = bpf_map__fd(obj->maps.argmap);
	args.targ_tgid = env.pid;
	args.targ_pid = env.tid;
	bpf_map_update_elem(map_fd, &i, &args, 0);
	if (err) {
		fprintf(stderr, "Failed to update flag map\n");
		goto cleanup;
	}

	err = kvmexittime_bpf__attach(obj);
	if (err) {
		fprintf(stderr, "failed to attach BPF programs\n");
		goto cleanup;
	}

	if (signal(SIGINT, sig_int) == SIG_ERR) {
		fprintf(stderr, "can't set signal handler: %s\n", strerror(errno));
		err = 1;
		goto cleanup;
	}

	printf("Tracing kvm exit...  Hit Ctrl-C to end.\n");

	sleep(env.interval);

	printf("\n%-18s\t%-16s\t%-16s\t%-16s\t%-16s\n", "Exit reason", "Count", "E2E-time", "Sched-out-time", "On-cpu-time");

	fd = bpf_map__fd(obj->maps.counts);
	while (!bpf_map_get_next_key(fd, &lookup_key, &next_key)) {
		err = bpf_map_lookup_elem(fd, &next_key, &tm);
		if (err < 0) {
			fprintf(stderr, "failed to lookup exec: %d\n", err);
			return -1;
		
		}
		lookup_key = next_key;
		total_cnt += tm.count;
		total_ct += tm.cumulative_time;
		total_sot += tm.cumulative_sched_time;
		total_oct += tm.cumulative_time - tm.cumulative_sched_time;

		printf("%-18s\t%-16lld\t%-16llu\t%-16llu\t%-16llu\n", exit_reason_names[next_key], tm.count, tm.cumulative_time, tm.cumulative_sched_time, tm.cumulative_time - tm.cumulative_sched_time);
	}

	printf("%-18s\t%-16lld\t%-16llu\t%-16llu\t%-16llu\n", "Total", total_cnt, total_ct, total_sot, total_oct);

cleanup:
	kvmexittime_bpf__destroy(obj);

	return err != 0;
}
