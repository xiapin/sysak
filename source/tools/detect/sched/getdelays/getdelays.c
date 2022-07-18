#include <argp.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include "getdelays.h"
#include "stat_nl.h"
#include "pidComm.h"
#include "bpf/getdelays.skel.h"

unsigned int nr_cpus;
FILE *filep = NULL;
static volatile sig_atomic_t exiting = 0;
char log_dir[] = "/var/log/sysak/getdelays/";
char summ_path[] = "/var/log/sysak/getdelays/getdelays.log";
char syscall_path[] = "/var/log/sysak/getdelays/getdelays_syscall.log";
char filename[256] = {0};

/* env.type = TASKSTATS_CMD_ATTR_PID or
 *  TASKSTATS_CMD_ATTR_TGID 
 **/

struct env {
	int type;
	pid_t pid;
	unsigned long span;
	__u64 threshold;
	bool verbose;
} env = {
	.span = 0,
	.verbose = false,
	.pid = 0,
	.type = TASKSTATS_CMD_ATTR_UNSPEC,
};

char *sys_array[MAX_NR];
struct syscalls_acct sys_cnts[1024];
int nr_to_syscall(int argc, char *arry[]);

const char *argp_program_version = "getdelays 0.1";
const char argp_program_doc[] =
"Get a task'delays cased by irq/IO/preempted/mm reclaim/mm swap.\n"
"\n"
"USAGE: getdelays [--help] <-t TGID|-P PID> [-f ./res.log] [span times]\n"
"\n"
"EXAMPLES:\n"
"    getdelays -p 123          # trace pid 123(use for threads only)\n"
"    getdelays -t 123          # trace tgid 123\n"
"    getdelays -p 123 -f a.log # record result to a.log (default to /var/log/sysak/getdelays.log)\n"
"    getdelays -p 123 10       # monitor for 10 seconds\n";

static const struct argp_option opts[] = {
	{ "pid", 'p', "PID", 0,  "Thread PID to trace"},
	{ "tid", 't', "TGID", 0, "Process TGID to trace"},
	{ "verbose", 'v', NULL, 0, "Verbose debug output" },
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

static int prepare_dictory(char *path)
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
	unsigned long span;

	switch (key) {
	case 'h':
		argp_state_help(state, stderr, ARGP_HELP_STD_HELP);
		break;
	case 'v':
		env.verbose = true;
		break;
	case 't':
		errno = 0;
		if (env.type == TASKSTATS_CMD_ATTR_PID) {
			argp_usage(state);
			return -EINVAL;
		}
		pid = strtol(arg, NULL, 10);
		if (errno || pid <= 0) {
			fprintf(stderr, "Invalid PID: %s\n", arg);
			argp_usage(state);
			return -EINVAL;
		}
		env.pid = pid;
		env.type = TASKSTATS_CMD_ATTR_TGID;
		break;
	case 'p':
		if (env.type == TASKSTATS_CMD_ATTR_TGID) {
			argp_usage(state);
			return -EINVAL;
		}
		errno = 0;
		pid = strtol(arg, NULL, 10);
		if (errno || pid < 0) {
			fprintf(stderr, "Invalid TID: %s\n", arg);
			argp_usage(state);
			return -EINVAL;
		}
		env.pid = pid;
		env.type = TASKSTATS_CMD_ATTR_PID;
		break;
	case 'f':
		if (strlen(arg) < 2)
			strncpy(filename, summ_path, sizeof(filename));
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
		span = strtoul(arg, NULL, 10);
		if (errno || span <= 0) {
			fprintf(stderr, "Invalid SPAN: %s\n", arg);
			argp_usage(state);
		}
		env.span = span;
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

void handle_event(void *ctx, int cpu, void *data, __u32 data_sz)
{
	struct event tmpe, *e;
	const struct event *ep = data;
	struct tm *tm;
	char ts[64];
	time_t t;

	tmpe = *ep;
	e = &tmpe;
	time(&t);
	tm = localtime(&t);
	strftime(ts, sizeof(ts), "%F %H:%M:%S", tm);
	e->delay = e->delay/(1000*1000);
	fprintf(filep, "%-21s %-6d %-16s %-8d %-10llu\n",
		ts, e->cpuid, e->task, e->pid, e->delay);
}

#define average_ms(t, c) (t / 1000000ULL / (c ? c : 1))
void account_syscall_delay(int fd, struct syscalls_acct *summary, struct syscalls_acct **accts)
{
	int i;
	FILE *fp;
	char tmp[64];
	struct syscalls_acct acct;
	struct syscall_key key, next_key;

	fp = fopen(syscall_path, "w+");
	if (!fp)
		fp = stderr;

	memset(sys_cnts, 0, sizeof(sys_cnts));
	while (bpf_map_get_next_key(fd, &key, &next_key) == 0) {
		if (bpf_map_lookup_elem(fd, &next_key, &acct))
			continue;
		if (next_key.sysid < 1023) {
			int idx = next_key.sysid;

			sys_cnts[idx].delay += acct.delay;
			sys_cnts[idx].cnt += acct.cnt;
			sys_cnts[idx].wait += acct.wait;
			sys_cnts[idx].sleep += acct.sleep;
		}
		summary->cnt += acct.cnt;
		summary->delay += acct.delay;
		summary->sleep += acct.sleep;
		summary->wait += acct.wait;
#if 0
		fprintf(fp, "%-15llu %-15llu %-15.3f %-15llu %-12llu %-12llu %-8d %-12d\n", 
			acct.delay/(1000*1000), acct.cnt,
			average_ms((double)acct.delay, acct.cnt),
			(acct.delay-acct.wait-acct.sleep)/(1000*1000),
			acct.wait/(1000*1000), 
			acct.sleep/(1000*1000),
			next_key.sysid, next_key.pid);
#endif
		key = next_key;
	}

	summary->sys = summary->delay - summary->sleep - summary->wait;
	fprintf(fp, "%-10s %-15s %-15s %-15s %-15s %-12s %-12s\n",
		"summary: ", "DELAY(ms)", "COUNT", "AVG(ms)", "SYS(ms)", "WAIT(ms)", "SLEEP(ms)");

	fprintf(fp, "%-10s %-15.3f %-15llu %-15.3f %-15.3f %-12.3f %-12.3f\n", 
		"         ", (double)(summary->delay)/(1000*1000), summary->cnt,
		average_ms((double)summary->delay, summary->cnt),
		(double)summary->sys/(1000*1000), (double)summary->wait/(1000*1000), 
		(double)summary->sleep/(1000*1000));
	fprintf(fp, "\n");

	fprintf(fp, "%-15s %-15s %-12s %-12s %-12s %-12s %-12s\n",
		"DELAY(ms)", "COUNT", "AVG(ms)", "SYS(ms)", "WAIT(ms)", "SLEEP(ms)", "SYSCALL");
	for (i = 0; i < 1024; i++) {
		if (!sys_cnts[i].cnt)
			continue;
		memset(tmp, 0, sizeof(tmp));
		if (sys_array[i])
			snprintf(tmp, sizeof(tmp), "%s(%d)", sys_array[i], i);
		else
			snprintf(tmp, sizeof(tmp), "NULL(%d)", i);
		fprintf(fp, "%-15.3f %-15llu %-12.3f %-12.3f %-12.3f %-12.3f %s\n", 
			(double)(sys_cnts[i].delay)/(1000*1000), sys_cnts[i].cnt,
			average_ms((double)sys_cnts[i].delay, sys_cnts[i].cnt),
			(double)(sys_cnts[i].delay-sys_cnts[i].wait-sys_cnts[i].sleep)/(1000*1000),
			(double)(sys_cnts[i].wait)/(1000*1000), 
			(double)(sys_cnts[i].sleep)/(1000*1000), tmp);
	}
}

void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt)
{
	printf("Lost %llu events on CPU #%d!\n", lost_cnt, cpu);
}

struct msgtemplate msg1, msg2;
int get_nl_stats(int type, pid_t pid, struct msgtemplate *msg, struct taskstats **tstats);
void print_delayacct_sum(FILE *out, struct taskstats *t1, struct taskstats *t2,
			struct irq_acct *irqst, struct syscalls_acct *sysacct);

int main(int argc, char **argv)
{
	struct irq_acct irqdelay;
	struct syscalls_acct syssum, *sysaccts;
	struct taskstats *stats1, *stats2, *tstats;
	int i, err, arg_fd, irq_fd, sys_fd;
	static const struct argp argp = {
		.options = opts,
		.parser = parse_arg,
		.doc = argp_program_doc,
	};
	struct perf_buffer *pb = NULL;
	struct getdelays_bpf *obj;
	struct perf_buffer_opts pb_opts = {};
	struct args args = {};

	err = prepare_dictory(log_dir);
	if (err)
		return err;

	err = argp_parse(&argp, argc, argv, 0, NULL, NULL);
	if (err)
		return err;
	if (!filep) {
		filep = fopen(summ_path, "w+");
		if (!filep) {
			err = errno;
			fprintf(stderr, "%s :fopen %s\n",
				strerror(errno), summ_path);
			return err;
		}
	}

	libbpf_set_print(libbpf_print_fn);
	bump_memlock_rlimit();
	
	obj = getdelays_bpf__open();
	if (!obj) {
		fprintf(stderr, "failed to open BPF object\n");
		return 1;
	}

	err = getdelays_bpf__load(obj);
	if (err) {
		fprintf(stderr, "failed to load BPF object: %d\n", err);
		goto cleanup;
	}

	i = 0;
	irq_fd = bpf_map__fd(obj->maps.irqmap);
	sys_fd = bpf_map__fd(obj->maps.syscmap);
	arg_fd = bpf_map__fd(obj->maps.argmap);
	args.type = env.type;
	args.targ_pid = env.pid;
	args.threshold = env.threshold;

	pb_opts.sample_cb = handle_event;
	pb = perf_buffer__new(bpf_map__fd(obj->maps.events), 64, &pb_opts);
	if (!pb) {
		err = errno;
		fprintf(stderr, "failed to open perf buffer: %d\n", err);
		goto cleanup;
	}

	if (signal(SIGINT, sig_int) == SIG_ERR ||
		signal(SIGALRM, sig_alarm) == SIG_ERR) {
		err = errno;
		fprintf(stderr, "can't set signal handler: %s\n", strerror(errno));
		goto cleanup;
	}

	if (env.span)
		alarm(env.span);

	err = bpf_map_update_elem(arg_fd, &i, &args, 0);
	if (err) {
		fprintf(stderr, "Failed to update flag map\n");
		goto cleanup;
	}
	memset(&irqdelay, 0, sizeof(irqdelay));
	err = bpf_map_update_elem(irq_fd, &args.targ_pid, &irqdelay, 0);
	if (err) {
		fprintf(stderr, "Failed to update flag map\n");
		goto cleanup;
	}

	memset(sys_array, 0, sizeof(sys_array));
	err = nr_to_syscall(0, sys_array);

	err = get_nl_stats(env.type, env.pid, &msg1, &tstats);
	if (err)
		goto cleanup;
	stats1 = tstats;

	err = getdelays_bpf__attach(obj);
	if (err) {
		fprintf(stderr, "failed to attach BPF programs\n");
		goto cleanup;
	}

	while (!exiting) {
		err = perf_buffer__poll(pb, 100);
		if (err < 0 && err != -EINTR) {
			fprintf(stderr, "error polling perf buffer: %s\n", strerror(-err));
			goto cleanup;
		}
		/* reset err to return 0 if exiting */
		err = 0;
	}

	err = get_nl_stats(env.type, args.targ_pid, &msg2, &tstats);
	if (err)
		goto cleanup;
	stats2 = tstats;
	err = bpf_map_lookup_elem(irq_fd, &args.targ_pid, &irqdelay);
	if (err) {
		fprintf(stderr, "bpf_map_lookup_elem: %s\n", strerror(-err));
		memset(&irqdelay, 0, sizeof(irqdelay));
	}
	memset(&syssum, 0, sizeof(syssum));
	account_syscall_delay(sys_fd, &syssum, &sysaccts);

	print_delayacct_sum(filep, stats1, stats2, &irqdelay, &syssum);
cleanup:
	perf_buffer__free(pb);
	getdelays_bpf__destroy(obj);

	return err != 0;
}
