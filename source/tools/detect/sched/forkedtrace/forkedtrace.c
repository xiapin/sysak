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
#include "bpf/forkedtrace.skel.h"
#include "forkedtrace.h"

#define SIZE_512MB	(512*1024*1024)
unsigned int nr_cpus;
static __u64 last_new, last_wk;
FILE *filep = NULL;
char filename[256] = {0};
char log_dir[] = "/var/log/sysak/forkedtrace";
char defaultfile[] = "/var/log/sysak/forkedtrace/forkedtrace.log";

static volatile sig_atomic_t exiting;
char *help_str = "sysak forkedtrace";
struct env {
	__u32 thresh;
	bool verbose;
	struct wake_account *wact;
} env = {
	.thresh = 100,
	.verbose = false,
	.wact = NULL,
};

static void usage(char *prog)
{
	const char *str =
	"  Usage: %s [OPTIONS]\n"
	"  Options:\n"
	"  -t THRESH       specify the forked threshold times, default=100\n"
	"  -f result.log        result file, default is /var/log/sysak/forkedtrace.log\n"
	"  -s TIME              specify how long to run \n"
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

static int prepare_dictory(char *path)
{
	int ret;

	ret = mkdir(path, 0777);
	if (ret < 0 && errno != EEXIST)
		return errno;
	else
		return 0;
}

void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt)
{
	printf("Lost %llu events on CPU #%d!\n", lost_cnt, cpu);
}

static void check_file_overflow(const char *path, FILE *fp, bool reset)
{
	struct stat buf;

	stat(path, &buf);
	if ((buf.st_size > SIZE_512MB) && reset) {
		printf("DEBUG:rewind\n");
		rewind(fp);
	}
}

int iterate_wake_new(int fd, FILE *fp)
{
	__u64 total = 0;
	int err;
	struct wake_up_data ttwu;
	__u32 lookup_key = -1, next_key;

	while (!bpf_map_get_next_key(fd, &lookup_key, &next_key)) {
		err = bpf_map_lookup_elem(fd, &next_key, &ttwu);

		err = bpf_map_delete_elem(fd, &next_key);
		if (err < 0) {
			fprintf(stderr, "failed to delete elem: %d\n", err);
			return -1;
		}
		lookup_key = next_key;

		if (!next_key)
			continue;

		fprintf(fp, "%s[%d]:ppid=%d:cnt=%llu ", ttwu.comm, 
				ttwu.wakee, ttwu.ppid, ttwu.new_cnt);
		total = total + ttwu.new_cnt;	//for debug
	}
	if (total > 0)
		fprintf(fp, "<total=%llu>\n\n", total);

	return 0;
}

int warn_fork(int cntfd, FILE *filep, __u64 delta[], struct env* env)
{
	int i, ret = 0;
	__u32 key = 0;
	__u64 delta_new, delta_wk, sum_new = 0, sum_wk = 0;
	struct wake_account *wact, *tmp;

	wact = env->wact;

	ret = bpf_map_lookup_elem(cntfd, &key, wact);
	if (ret < 0) {
		fprintf(stderr, "failed to lookup count fd: %d\n", ret);
		return ret;
	}

	for (i = 0; i < nr_cpus; i++) {
		tmp = &wact[i];
		sum_new += tmp->new_cnt;
		sum_wk += tmp->wake_cnt;
	}
	delta_new = sum_new - last_new;
	last_new = sum_new;
	if (delta_new > env->thresh && last_new) {
		ret = 1;
		delta[0] = delta_new;
	}
	delta_wk = sum_wk - last_wk;
	last_wk = sum_wk;
	if (delta_wk > env->thresh && last_wk) {
		ret |= 2;
		delta[1] = delta_wk;
	}
	return ret;
}

int check_warn(struct fds *fds, FILE* filep, struct env* env)
{
	int ret;
	char ts[64];
	time_t t;
	struct tm *tm;
	__u64 delta[2];

	time(&t);
	tm = localtime(&t);
	strftime(ts, sizeof(ts), "%F %H:%M:%S", tm);

	ret = warn_fork(fds->cntfd, filep, delta, env);
	if (ret <= 0)
		return ret;
	if (ret & 1) {
		fprintf(filep, "####%s forked=%llu\n", ts, delta[0]);
		iterate_wake_new(fds->forkfd, filep);
	}

	if (ret & 2) {
		;/* fprintf(filep, "####%s wakeup=%llu\n", ts, delta[1]); */
	}
	return 0;
}

int forkedtrace_handler(struct fds *fds, struct forkedtrace_bpf *skel, struct env *env)
{
	int err = 0;

	/* Attach tracepoint handler */
	err = forkedtrace_bpf__attach(skel);
	if (err) {
		fprintf(stderr, "Failed to attach BPF skeleton\n");
		return err;
	}

	while (!exiting) {
		sleep(2);
		check_warn(fds, filep, env);
	}

	return err;
}

static void sig_exit(int signo)
{
	exiting = 1;
}

int main(int argc, char **argv)
{
	int err;
	struct fds fds;
	struct wake_account *wact;
	struct wake_account acct;
	struct forkedtrace_bpf *skel;
	int c, option_index;
	unsigned long span = 0;

	err = prepare_dictory(log_dir);
	if (err)
		return err;

	for (;;) {
		c = getopt_long(argc, argv, "t:f:s:S:vh", NULL, &option_index);
		if (c == -1)
			break;

		switch (c) {
			case 't':
				env.thresh = (int)strtoul(optarg, NULL, 10);
				if ((errno == ERANGE && (env.thresh == LONG_MAX || env.thresh == LONG_MIN))
					|| (errno != 0 && env.thresh == 0)) {
					fprintf(stderr, "%s :strtoul %s\n", strerror(errno), optarg);
					return errno;
				}
				env.thresh = env.thresh;
				break;
			case 'f':
				if (strlen(optarg) < 2)
					strncpy(filename, defaultfile, sizeof(filename));
				else
					strncpy(filename, optarg, sizeof(filename));
				filep = fopen(filename, "w+");
				if (!filep) {
					int ret = errno;
					fprintf(stderr, "%s :fopen %s\n",
					strerror(errno), filename);
					return ret;
				}
				break;
			case 's':
				span = (int)strtoul(optarg, NULL, 10);
				if ((errno == ERANGE && (span == LONG_MAX || span == LONG_MIN))
					|| (errno != 0 && span == 0)) {
					perror("strtoul");
					return errno;
				}
				break;
			case 'v':
				env.verbose = true;
				break;
			case 'h':
				usage(help_str);
				break;
			default:
				usage(help_str);
		}
	}
	if (!filep) {
		filep = fopen(defaultfile, "w+");
		if (!filep) {
			int ret = errno;
			fprintf(stderr, "%s :fopen %s\n",
				strerror(errno), defaultfile);
			return ret;
		}
		strncpy(filename, defaultfile, sizeof(filename));
	}
	
	wact = calloc(nr_cpus, sizeof(struct wake_account));
	if (!wact)
		return -errno;
	env.wact = wact;

	/* Set up libbpf errors and debug info callback */
	libbpf_set_print(libbpf_print_fn);
	/* Bump RLIMIT_MEMLOCK to allow BPF sub-system to do anything */
	bump_memlock_rlimit();

	nr_cpus = libbpf_num_possible_cpus();
	/* Open load and verify BPF application */
	skel = forkedtrace_bpf__open();
	if (!skel) {
		fprintf(stderr, "Failed to open BPF skeleton\n");
		return 1;
	}

	err = forkedtrace_bpf__load(skel);
	if (err) {
		fprintf(stderr, "Failed to load BPF skeleton\n");
		return 1;
	}

	fds.forkfd = bpf_map__fd(skel->maps.info_map);
	fds.cntfd = bpf_map__fd(skel->maps.cnt_map);
	if (signal(SIGINT, sig_exit) == SIG_ERR ||
		signal(SIGALRM, sig_exit) == SIG_ERR ||
		signal(SIGTERM, sig_exit) == SIG_ERR) {
		fprintf(stderr, "can't set signal handler: %s\n", strerror(errno));
		goto cleanup;
	}

	last_new = 0;
	last_wk = 0;
	memset(&acct, 0, sizeof(acct));
	bpf_map_update_elem(fds.cntfd, 0, &acct, BPF_ANY);
	if (span)
		alarm(span);

	err = forkedtrace_handler(&fds, skel, &env);

cleanup:
	//if (env.wact)
	//	free(env.wact);
	//forkedtrace_bpf__destroy(skel);
	return -err;
}
