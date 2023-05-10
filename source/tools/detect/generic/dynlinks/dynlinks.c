#include <argp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include "pidComm.h"
#include "sched_jit.h"
#include "dynlinks.h"
#include "./bpf/dynlinks.skel.h"

struct env {
	__u64 sample_period;
	time_t duration;
	bool verbose, summary;
	__u64 threshold;
	struct sched_jit_summary *sump;
	char *shm_p;
} env = {
	.duration = 0,
	.threshold = 10*1000*1000,	/* 10ms */
	.summary = false,
	.shm_p = NULL,
};

static int nr_cpus;
FILE *filep = NULL;
char filename[256] = {0};
char log_dir[] = "/var/log/sysak/dynlinks";
char defaultfile[] = "/var/log/sysak/dynlinks/dynlinks.log";

volatile sig_atomic_t exiting = 0;

const char *argp_program_version = "dynlinks 0.1";
const char argp_program_doc[] =
"Catch the irq-off time more than threshold.\n"
"\n"
"USAGE: dynlinks [--help] [-t THRESH(ms)] [-S SHM] [-f LOGFILE] [duration(s)]\n"
"\n"
"EXAMPLES:\n"
"    dynlinks                # run forever, and detect dynlinks more than 10ms(default)\n"
"    dynlinks -S shmkey      # record summary log to share memory shmkey\n"
"    dynlinks -t 15          # detect dynlinks with threshold 15ms (default 10ms)\n"
"    dynlinks -f a.log       # record result to a.log (default to ~sysak/dynlinks/dynlinks.log)\n";

static const struct argp_option opts[] = {
	{ "threshold", 't', "THRESH", 0, "Threshold to detect, default 10ms"},
	{ "logfile", 'f', "LOGFILE", 0, "logfile for result"},
	{ "summary", 'S', "SHMKEY", 0, "Record summary log to share memory"},
	{ "verbose", 'v', NULL, 0, "Verbose debug output"},
	{ NULL, 'h', NULL, OPTION_HIDDEN, "Show the full help"},
	{},
};

static error_t parse_arg(int key, char *arg, struct argp_state *state)
{
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
		} else if (thresh < 5) {
			fprintf(stderr, "threshold must >5ms, set to default 10ms\n");
			break;
		}
		env.threshold = thresh * 1000*1000;
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

	/* refer to watchdog.c:set_sample_period, sample_period set to thres*2/5. */
	env.sample_period = env.threshold*2/5;

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

void dynlinks_handler(int ifd, int tfd, struct dynlinks_bpf *obj)
{
	__u64 total = 0;
	__u64 lookup_key = -1, next_key, i;
	struct value *p;
	int err = 0;

	p = calloc(sizeof(struct value), nr_cpus);
	if (!p)
		return;

	err = dynlinks_bpf__attach(obj);
	if (err) {
		fprintf(stderr, "Failed to attach BPF skeleton\n");
		return err;
	}
	while (!exiting) {
		int pid;
		sleep(3);
		printf("---------\n");
		while (!(err = bpf_map_get_next_key(ifd, &lookup_key, &next_key))) {
			err = bpf_map_lookup_elem(ifd, &next_key, p);
			lookup_key = next_key;
			if (err < 0) {
				printf("lookup elem key=%d fail\n", next_key);
				continue;
			}
			total = 0;
			for (i=0; i<nr_cpus; i++) {
				total += p[i].cnt; 
			}
			printf("pid=%d Rload=%lld\n", lookup_key, total);
		}
	}
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

static void sig_int(int sig)
{
	exiting = 1;
}

int main(int argc, char **argv)
{
	int err, tfd, ifd, arg_fd;
	struct dynlinks_bpf *obj;
	static const struct argp argp = {
		.options = opts,
		.parser = parse_arg,
		.doc = argp_program_doc,
	};

	nr_cpus = libbpf_num_possible_cpus();
	if (nr_cpus < 0) {
		fprintf(stderr, "failed to get # of possible cpus: '%s'!\n",
			strerror(-nr_cpus));
		return 1;
	}

	err = argp_parse(&argp, argc, argv, 0, NULL, NULL);
	if (err) {
		fprintf(stderr, "argp_parse fail\n");
		return err;
	}

	libbpf_set_print(libbpf_print_fn);

	bump_memlock_rlimit();
	if (err) {
		fprintf(stderr, "Failed to load kallsyms\n");
		return err;
	}

	obj = dynlinks_bpf__open_and_load();
	if (!obj) {
		fprintf(stderr, "failed to open and/or load BPF object\n");
		goto cleanup;
	}

	arg_fd = bpf_map__fd(obj->maps.arg_map);
	ifd = bpf_map__fd(obj->maps.info_map);
	tfd = bpf_map__fd(obj->maps.test_map);
	printf("ifd=%d tfd=%d\n", ifd, tfd);
	if (signal(SIGINT, sig_int) == SIG_ERR ||
		signal(SIGALRM, sig_alarm) == SIG_ERR) {
		fprintf(stderr, "can't set signal handler: %s\n", strerror(errno));
		err = 1;
		goto cleanup;
	}

	if (env.duration)
		alarm(env.duration);

	dynlinks_handler(ifd, tfd, obj);

cleanup:
	dynlinks_bpf__destroy(obj);
	return err != 0;
}

