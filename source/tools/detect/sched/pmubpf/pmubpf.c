#define _GNU_SOURCE
#include <argp.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sched.h>
#include <assert.h>
#include <error.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <asm/unistd.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include "pmubpf.h"
#include "bpf/pmubpf.skel.h"

#define SAMPLE_PERIOD	(1000*1000*1000)

unsigned int nr_cpus;
int pmufds[512];
FILE *filep = NULL;
int map_fd[3];
static volatile sig_atomic_t exiting = 0;
char log_dir[] = "/var/log/sysak/pmubpf/";
char defaultfile[] = "/var/log/sysak/pmubpf/pmubpf.log";
char filename[256] = {0};

struct env {
	pid_t pid;
	pid_t tid;
	unsigned long span;
	__u64 threshold;
	bool previous;
	bool verbose;
	bool summary;
	struct sched_jit_summary *sump;
	char *shm_p;
} env = {
	.span = 0,
	.threshold = 50*1000*1000,
	.shm_p = NULL,
};

const char *argp_program_version = "pmubpf 0.1";
const char argp_program_doc[] =
"Trace high run queue latency.\n"
"\n"
"USAGE: pmubpf [-h] [-s SPAN] [-t TID] [-S SUM] [-f LOG] [-P] [threshold]\n"
"\n"
"EXAMPLES:\n"
"    pmubpf           # trace latency higher than 50ms (default)\n"
"    pmubpf -f a.log  # record result to a.log (default ~/sysak/pmubpf/pmubpf.log)\n"
"    pmubpf 12        # trace latency higher than 12 ms\n"
"    pmubpf -p 123    # trace pid 123\n"
"    pmubpf -t 123    # trace tid 123 (use for threads only)\n"
"    pmubpf -s 10     # monitor for 10 seconds\n"
"    pmubpf -S shmkey # Record summary log to share memory shmkey\n"
"    pmubpf -P        # also show previous task name and TID\n";

static const struct argp_option opts[] = {
	{ "pid", 'p', "PID", 0, "Process PID to trace"},
	{ "tid", 't', "TID", 0, "Thread TID to trace"},
	{ "span", 's', "SPAN", 0, "How long to run"},
	{ "verbose", 'v', NULL, 0, "Verbose debug output" },
	{ "summary", 'S', "SHMKEY", 0, "record summary log to share memory" },
	{ "previous", 'P', NULL, 0, "also show previous task name and TID" },
	{ "logfile", 'f', "LOGFILE", 0, "logfile for result"},
	{ NULL, 'h', NULL, OPTION_HIDDEN, "Show the full help" },
	{},
};

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

static int prepare_directory(char *path)
{
	int ret;

	ret = mkdir(path, 0777);
	if (ret < 0 && errno != EEXIST)
		return errno;
	else
		return 0;
}

static error_t parse_arg(int key, char *arg, struct argp_state *state)
{
	static int pos_args;
	int pid;
	long long threshold;
	unsigned long span;

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
		pid = strtol(arg, NULL, 10);
		if (errno || pid <= 0) {
			fprintf(stderr, "Invalid TID: %s\n", arg);
			argp_usage(state);
		}
		env.tid = pid;
		break;
	case 's':
		errno = 0;
		span = strtoul(arg, NULL, 10);
		if (errno || span <= 0) {
			fprintf(stderr, "Invalid SPAN: %s\n", arg);
			argp_usage(state);
		}
		env.span = span;
		break;
	case 'f':
		if (strlen(arg) < 2)
			strncpy(filename, defaultfile, sizeof(filename));
		else
			strncpy(filename, arg, sizeof(filename));
		filep = fopen(filename, "w+");
		if (!filep) {
			int ret = errno;
			fprintf(stderr, "%s :fopen %s\n",
				strerror(errno), filename);
			return ret;
		}
		break;
	case ARGP_KEY_ARG:
		if (pos_args++) {
			fprintf(stderr,
				"Unrecognized positional argument: %s\n", arg);
			argp_usage(state);
		}
		errno = 0;
		threshold = strtoll(arg, NULL, 10);
		if (errno || threshold <= 0) {
			fprintf(stderr, "Invalid delay (in ms): %s\n", arg);
			argp_usage(state);
		}
		env.threshold = threshold*1000*1000;
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
	if (level == LIBBPF_DEBUG && !env.verbose)
		return 0;
	return vfprintf(stderr, format, args);
}

static void sig_alarm(int signo)
{
	exiting = 1;
}

static void sig_int(int signo)
{
	exiting = 1;
}

void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt)
{
	printf("Lost %llu events on CPU #%d!\n", lost_cnt, cpu);
}

static long
sys_perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
		int cpu, int group_fd, unsigned long flags)
{
	int ret;

	ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
			group_fd, flags);
	return ret;
}

static int check_on_cpu(int cpu, struct perf_event_attr *attr)
{
	/* __u64 value; */
	int pmu_fd, error = 0;
	cpu_set_t set;

	CPU_ZERO(&set);
	CPU_SET(cpu, &set);
	assert(sched_setaffinity(0, sizeof(set), &set) == 0);

	pmu_fd = sys_perf_event_open(attr, -1/*pid*/, cpu/*cpu*/, -1/*group_fd*/, 0);
	if (pmu_fd < 0) {
		fprintf(stderr, "sys_perf_event_open failed on CPU %d\n", cpu);
		error = 1;
		goto on_exit;
	}
	pmufds[cpu] = pmu_fd;
	assert(bpf_map_update_elem(map_fd[0], &cpu, &pmu_fd, BPF_ANY) == 0);
	assert(ioctl(pmu_fd, PERF_EVENT_IOC_ENABLE, 0) == 0);
#if 0
	sleep(1);
	/* Check the value */
	if (bpf_map_lookup_elem(map_fd[1], &cpu, &value)) {
		fprintf(stderr, "Value missing for CPU %d\n", cpu);
		error = 1;
		goto on_exit;
	} else {
		fprintf(stderr, "CPU %d: %llu\n", cpu, value);
	}
#endif
	return 0;

on_exit:
	return error;
}

static void test_perf_event_array(struct perf_event_attr *attr,
				  const char *name)
{
	int i;
	int err = 0;

	printf("Test reading %s counters\n", name);

	for (i = 0; i < nr_cpus; i++) {
		err = check_on_cpu(i, attr);
		if (err)
			printf(" check_on_cpu %d failed\n", i);
	}
#if 0
	for (i = 0; i < nr_cpus; i++) {
		assert(waitpid(pid[i], &status, 0) == pid[i]);
		err |= status;
	}
#endif
	if (err)
		printf("Test: %s FAILED\n", name);
}

static void test_bpf_perf_event(void)
{
#if 0
	struct perf_event_attr attr_cycles = {
		.freq = 0,
		.sample_period = 1000*1000*1000, //SAMPLE_PERIOD,
		.inherit = 0,
		.type = PERF_TYPE_HARDWARE,
		.read_format = 0,
		.sample_type = 0,
		.config = PERF_COUNT_HW_CPU_CYCLES,
	};
	struct perf_event_attr attr_raw = {
		.freq = 0,
		.sample_period = SAMPLE_PERIOD,
		.inherit = 0,
		.type = PERF_TYPE_RAW,
		.read_format = 0,
		.sample_type = 0,
		/* Intel Instruction Retired */
		.config = 0xc0,
	};
#endif
	struct perf_event_attr attr_clock = {
		.freq = 0,
		.sample_period = SAMPLE_PERIOD,
		.inherit = 0,
		.type = PERF_TYPE_SOFTWARE,
		.read_format = 0,
		.sample_type = 0,
		.config = PERF_COUNT_SW_CPU_CLOCK,
	};
#if 0
	struct perf_event_attr attr_l1d_load = {
		.freq = 0,
		.sample_period = SAMPLE_PERIOD,
		.inherit = 0,
		.type = PERF_TYPE_HW_CACHE,
		.read_format = 0,
		.sample_type = 0,
		.config =
			PERF_COUNT_HW_CACHE_L1D |
			(PERF_COUNT_HW_CACHE_OP_READ << 8) |
			(PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),
	};
	struct perf_event_attr attr_llc_miss = {
		.freq = 0,
		.sample_period = SAMPLE_PERIOD,
		.inherit = 0,
		.type = PERF_TYPE_HW_CACHE,
		.read_format = 0,
		.sample_type = 0,
		.config =
			PERF_COUNT_HW_CACHE_LL |
			(PERF_COUNT_HW_CACHE_OP_READ << 8) |
			(PERF_COUNT_HW_CACHE_RESULT_MISS << 16),
	};
	struct perf_event_attr attr_msr_tsc = {
		.freq = 0,
		.sample_period = 0,
		.inherit = 0,
		/* From /sys/bus/event_source/devices/msr/ */
		.type = 7,
		.read_format = 0,
		.sample_type = 0,
		.config = 0,
	};
#endif
	test_perf_event_array(&attr_clock, "SOFTWARE-clock");
#if 0
	test_perf_event_array(&attr_raw, "RAW-instruction-retired");
	test_perf_event_array(&attr_l1d_load, "HW_CACHE-L1D-load");
	test_perf_event_array(&attr_cycles, "HARDWARE-cycles");

	/* below tests may fail in qemu */
	test_perf_event_array(&attr_llc_miss, "HW_CACHE-LLC-miss");
#endif
}

int output_task_counter(void)
{
	int cpu, err = 0;
	__u64 sum = 0, value = 0;

	for (cpu = 0; cpu < nr_cpus; cpu++) {
		if (bpf_map_lookup_elem(map_fd[1], &cpu, &value)) {
			fprintf(stderr, "Value missing for CPU %d\n", cpu);
			err = 1;
			return err;
		} else {
			/*fprintf(stderr, "CPU %d: %llu\n", cpu, value);*/
			sum = sum+value;
		}
	}
	fprintf(stderr, "count = %llu\n", sum);
	return 0;
}

void output_cgroup_counter(void)
{
	__u64 *val, sum = 0;
	int i, err = 0;
	struct cg_key lookup_key = {}, next_key;

	val = calloc(nr_cpus, sizeof(__u64));
	if (!val)
		return;
	while (!bpf_map_get_next_key(map_fd[2], &lookup_key, &next_key)) {
		err = bpf_map_lookup_elem(map_fd[2], &next_key, val);
		if (err) {
			printf("lookup elem fail\n");
		}
		lookup_key = next_key;
		for (i = 0; i < nr_cpus; i++)
			sum += val[i];
		printf("cgid[%llu] counter=%llu\n",
			lookup_key.cgid, sum);
	}
}

int main(int argc, char **argv)
{
	int i, err, arg_fd, cpu;
	static const struct argp argp = {
		.options = opts,
		.parser = parse_arg,
		.doc = argp_program_doc,
	};
	struct pmubpf_bpf *obj;
	struct args args = {};

	err = prepare_directory(log_dir);
	if (err)
		return err;

	err = argp_parse(&argp, argc, argv, 0, NULL, NULL);
	if (err)
		return err;
	if (!filep) {
		filep = fopen(defaultfile, "w+");
		if (!filep) {
			err = errno;
			fprintf(stderr, "%s :fopen %s\n",
				strerror(errno), defaultfile);
			return err;
		}
	}

	libbpf_set_print(libbpf_print_fn);
	bump_memlock_rlimit();

	obj = pmubpf_bpf__open();
	if (!obj) {
		fprintf(stderr, "failed to open BPF object\n");
		return 1;
	}

	err = pmubpf_bpf__load(obj);
	if (err) {
		fprintf(stderr, "failed to load BPF object: %d\n", err);
		goto cleanup;
	}

	i = 0;
	arg_fd = bpf_map__fd(obj->maps.argmap);
	args.targ_tgid = env.pid;
	args.targ_pid = env.tid;
	args.filter_pid = getpid();
	args.threshold = env.threshold;

	map_fd[0] = bpf_map__fd(obj->maps.event);
	map_fd[1] = bpf_map__fd(obj->maps.task_counter);
	map_fd[2] = bpf_map__fd(obj->maps.cg_counter);
	if (!env.summary) {
		if (env.previous)
			fprintf(filep, "%-21s %-6s %-16s %-8s %-10s %-16s %-6s\n",
				"TIME(runslw)", "CPU", "COMM", "TID", "LAT(ms)", "PREV COMM", "PREV TID");
		else
			fprintf(filep, "%-21s %-6s %-16s %-8s %-10s\n",
				"TIME(runslw)", "CPU", "COMM", "TID", "LAT(ms)");
	}

	if (signal(SIGINT, sig_int) == SIG_ERR ||
		signal(SIGALRM, sig_alarm) == SIG_ERR) {
		fprintf(stderr, "can't set signal handler: %s\n", strerror(errno));
		err = 1;
		goto cleanup;
	}

	if (env.span)
		alarm(env.span);
	nr_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	bpf_map_update_elem(arg_fd, &i, &args, 0);
	if (err) {
		fprintf(stderr, "Failed to update flag map\n");
		goto cleanup;
	}

	err = pmubpf_bpf__attach(obj);
	if (err) {
		fprintf(stderr, "failed to attach BPF programs\n");
		goto cleanup;
	}

	test_bpf_perf_event();

	while (!exiting) {
		sleep(1);
		output_task_counter();
		output_cgroup_counter();
	}


cleanup:
	pmubpf_bpf__destroy(obj);
	for (cpu = 0; cpu < nr_cpus; cpu++) {
		ioctl(pmufds[cpu], PERF_EVENT_IOC_DISABLE, 0);
		close(pmufds[cpu]);
	}
	return err != 0;
}
