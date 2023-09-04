#include <argp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include "tracesig.h"
#include "./bpf/tracesig.skel.h"

char log_dir[] = "/var/log/sysak/tracesig/";
char defaultfile[] = "/var/log/sysak/tracesig/tracesig.log";
char filename[256] = {0};

struct env {
	int path_fd;
	time_t duration;
	bool verbose;
	struct filter filter;
	FILE *filep;
} env = {
	.path_fd = 0,
	.duration = 0,
	.filep = NULL,
};

volatile sig_atomic_t exiting = 0;

const char *argp_program_version = "tracesig 0.1";
const char argp_program_doc[] =
"Catch the delay of a syscall more than threshold.\n"
"\n"
"USAGE: tracesig [--help] <[-f a.log]> duration\n"
"\n"
"EXAMPLES:\n"
"    tracesig            # run forever, detect delay more than 10ms(default)\n"
"    tracesig 10         # run for 10 seconds\n"
"    tracesig -c bash    # check the victim who's name is bash\n"
"    tracesig -f a.log   #save the log to a.log\n";

static const struct argp_option opts[] = {
	{ "file", 'f', "FILE", 0, "log file"},
	{ "comm", 'c', "COMM", 0, "Name of the victim"},
	{ "verbose", 'v', NULL, 0, "Verbose debug output" },
	{ NULL, 'h', NULL, OPTION_HIDDEN, "Show the full help" },
	{},
};

static error_t parse_arg(int key, char *arg, struct argp_state *state)
{
	FILE *old;
	int ret = errno;
	static int pos_args;

	switch (key) {
	case 'h':
		argp_state_help(state, stderr, ARGP_HELP_STD_HELP);
		break;
	case 'v':
		env.verbose = true;
		break;
	case 'f':
		old = env.filep;
		strncpy(filename, arg, sizeof(filename)-1);
		env.filep = fopen(filename, "w+");
		if (!env.filep) {
			int ret = errno;
			fprintf(stderr, "%s :fopen %s\n",
				strerror(errno), filename);
			return ret;
		}
		if (old)
			fclose(old);
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
		env.filter.inited = 1;
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

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
	if (level == LIBBPF_DEBUG && !env.verbose)
		return 0;
	return vfprintf(stderr, format, args);
}

static void sig_exit(int signo)
{
	exiting = 1;
}

char cmdline[256];
int read_cmdline(int pid, char buf[], int len)
{
	int i, ret, fd, n;
	char path[128];

	memset(path, 0, sizeof(path));
	snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
	fd = open(path, O_RDONLY);
	if (fd > 0) {
		memset(buf, 0, len);
		n = read(fd, buf, len);
		for (i = 0; i < n; i++)
			if (buf[i]=='\0')
				buf[i] = ' ';
		close(fd);
		ret = 0;
	} else {
		ret = -errno;
	}
	return ret;
}

void handle_event(void *ctx, int cpu, void *data, __u32 data_sz)
{
	int i;
	time_t t;
	char ts[64];
	struct tm *tm;
	struct mypathbuf buf;
	const struct mypathbuf *ebuf = data;

	if (ebuf->idx == END_MAGIC) {
		read_cmdline(ebuf->ppid, cmdline, 256);
		time(&t);
		tm = localtime(&t);
		strftime(ts, sizeof(ts), "%F %H:%M:%S", tm);
		fprintf(env.filep, "kill event happend at %21s\n", ts);
		fprintf(env.filep, "%s[%d] kill -%d to %s[%d]\n",
			ebuf->comm, ebuf->pid, ebuf->signum,
			ebuf->dstcomm, ebuf->dstpid);
		fprintf(env.filep, "murderer information:\n cwd:");

		for (i = 0; i < 16; i++) {
			if (bpf_map_lookup_elem(env.path_fd, &i, &buf) == 0) {
				if (strlen(buf.d_iname) < 1)
					continue;
				if (!strcmp(buf.d_iname, "/"))
					fprintf(env.filep, "%s", buf.d_iname);
				else
					fprintf(env.filep, "%s/", buf.d_iname);
			}
		}

		fprintf(env.filep, "\n cmdline=%s\n", cmdline);
		fprintf(env.filep, " parent=%s[%d]\n", ebuf->parent, ebuf->ppid);
		fprintf(env.filep, "\n");
		fflush(env.filep);
	}
}

int main(int argc, char **argv)
{
	int err, filter_fd, path_fd, i = 0;
	struct tracesig_bpf *obj;
	struct perf_buffer *pb = NULL;
	struct perf_buffer_opts pb_opts = {};

	static const struct argp argp = {
		.options = opts,
		.parser = parse_arg,
		.doc = argp_program_doc,
	};

	env.filep = fopen(filename, "w+");

	memset(&env.filter, 0, sizeof(env.filter));
	err = argp_parse(&argp, argc, argv, 0, NULL, NULL);
	if (err) {
		fprintf(stderr, "argp_parse fail\n");
		return err;
	}
	if (!env.filep) {
		fprintf(stderr, "ERROR: no file to log, change to stdout\n");
		env.filep = stdout;
	}
	libbpf_set_print(libbpf_print_fn);

	bump_memlock_rlimit();

	obj = tracesig_bpf__open_and_load();
	if (!obj) {
		fprintf(stderr, "failed to open and/or load BPF object\n");
		goto cleanup;
	}

	pb_opts.sample_cb = handle_event;
	pb = perf_buffer__new(bpf_map__fd(obj->maps.events), 64, &pb_opts);
	if (!pb) {
		err = -errno;
		fprintf(stderr, "failed to open perf buffer: %d\n", err);
		goto cleanup;
	}

	if (signal(SIGINT, sig_exit) == SIG_ERR ||
		signal(SIGALRM, sig_exit) == SIG_ERR ||
		signal(SIGTERM, sig_exit) == SIG_ERR) {
		fprintf(stderr, "can't set signal handler: %s\n", strerror(errno));
		err = 1;
		goto cleanup;
	}

	if (env.duration)
		alarm(env.duration);

	filter_fd = bpf_map__fd(obj->maps.filtermap);
	err = bpf_map_update_elem(filter_fd, &i, &env.filter, 0);
	if (err) {
		fprintf(stderr, "Failed to update filter map\n");
		goto cleanup;
	}

	path_fd = bpf_map__fd(obj->maps.path_map);
	env.path_fd = path_fd;
	err = tracesig_bpf__attach(obj);
	if (err) {
		fprintf(stderr, "Failed to attach BPF skeleton\n");
		return err;
	}

	while(!exiting) {
		err = perf_buffer__poll(pb, 100);
		if (err < 0 && err != -EINTR) {
			fprintf(stderr, "error polling perf buffer: %s\n", strerror(-err));
			goto cleanup;
		}
	};
cleanup:
	tracesig_bpf__destroy(obj);

	return err != 0;
}

