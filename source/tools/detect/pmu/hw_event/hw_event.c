#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <error.h>
#include <errno.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#define _GNU_SOURCE

#define NUM_ENV	(PERF_COUNT_HW_MAX+2)
#define ARGS_FMT(i, j)				\
	events_str[i], events_str[j],		\
	hw_events_cnt[i], hw_events_cnt[j],	\
	hw_events_cnt[j]==0?0:((double)hw_events_cnt[i]/hw_events_cnt[j])
#define EVENT_RATE(i, j)	\
	hw_events_cnt[j]==0?0:((double)hw_events_cnt[i]/hw_events_cnt[j])
char *path;
char fixpath[128];
char cmd[128], buffer[128];

__u64 hwconfigs[] = {
	PERF_COUNT_HW_CPU_CYCLES,
	PERF_COUNT_HW_INSTRUCTIONS,
	-1, //PERF_COUNT_HW_CACHE_REFERENCES,
	-1, //PERF_COUNT_HW_CACHE_MISSES,
	-1, /*PERF_COUNT_HW_BRANCH_INSTRUCTIONS*/
	-1, /*PERF_COUNT_HW_BRANCH_MISSES*/
	-1, /*PERF_COUNT_HW_BUS_CYCLES*/
	PERF_COUNT_HW_STALLED_CYCLES_FRONTEND,
	-1, /*PERF_COUNT_HW_STALLED_CYCLES_BACKEND*/
	-1, /* PERF_COUNT_HW_REF_CPU_CYCLES */
	PERF_COUNT_HW_CACHE_LL			<<  0  |
	(PERF_COUNT_HW_CACHE_OP_READ		<<  8) |
	(PERF_COUNT_HW_CACHE_RESULT_ACCESS	<< 16),
	PERF_COUNT_HW_CACHE_LL			<<  0  |
	(PERF_COUNT_HW_CACHE_OP_READ		<<  8) |
	(PERF_COUNT_HW_CACHE_RESULT_MISS	<< 16),
};

enum {
	PERF_COUNT_HW_CACHE_LL_R_ACCE = PERF_COUNT_HW_MAX,
	PERF_COUNT_HW_CACHE_LL_R_MISS,
};

char *events_str[] = {"cpu_cycles", "instructions", "cache_reference", "cache_miss",
			"branch_ins", "branch_miss", "bus_cycles", "stalled_cycles_frontend", 
			"stalled_cycles_backend", "ref_cpu_cycles", "llc_cache_ref", "llc_cache_miss"};
char origpath[]="/sys/fs/cgroup/perf_event/docker/";

static void usage(char *prog)
{
	const char *str =
	"  Usage: %s [-c container] [-s TIME]\n"
	"  Options:\n"
	"  -c container    container(docker) name or id, default all container \n"
	"  -s TIME         specify how long to run, default 5s \n"
	"eg. %s -c pause -s 10"
	;

	fprintf(stderr, str, prog, prog);
	exit(EXIT_FAILURE);
}

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
			   int cpu, int group_fd, unsigned long flags)
{
	int ret;

	ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
			group_fd, flags);	
	return ret;
}

char *help_str = "sysak hw_event";
int main(int argc, char *argv[])
{
	unsigned long flags = 0;
	int c, option_index,span = 5;
	int cpu = 0, pid = -1, cgrp_id = -1;
	int i, fd[NUM_ENV], cgrp_fd = -1;
	double cache_miss_rate, cpi, ins_stall_rate;
	unsigned long long hw_events_cnt[PERF_COUNT_HW_MAX];
	struct perf_event_attr attr1;
	struct perf_event_attr attr = {
		.type = PERF_TYPE_HARDWARE,
		.freq = 0,
		.disabled = 1,
		.sample_period = 1000*1000*1000,
	};

	path = origpath;
	for (;;) {
		FILE *result;
		c = getopt_long(argc, argv, "c:s:h", NULL, &option_index);
		if (c == -1)
			break;

		switch (c) {
			case 'c':
				memset(cmd, 0, sizeof(cmd));
				memset(buffer, 0, sizeof(buffer));
				snprintf(cmd, sizeof(cmd)-1, "docker inspect --format \"{{ .Id}}\" %s", optarg);
				result = popen(cmd, "r");

				if(fgets(buffer, sizeof(buffer)-1, result)) {
					buffer[64] = '\0';
					snprintf(fixpath, sizeof(fixpath),
						"/sys/fs/cgroup/perf_event/docker/%s/", buffer);
					if (!access(fixpath, F_OK))
						path = fixpath;
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
			case 'h':
				usage(help_str);
				break;
			default:
				usage(help_str);
		}
	}

	cgrp_id = open(path, O_RDONLY);
	if (cgrp_id < 0) {
		fprintf(stderr, "%s :open %s\n", strerror(errno), path);
		return cgrp_id;
	}

	for (i = 0; i < NUM_ENV; i++) {
		if ((long long)(hwconfigs[i]) < 0)
			continue;
		/* let's begin hw-cache events */
		if (i == PERF_COUNT_HW_MAX)
			attr.type = PERF_TYPE_HW_CACHE;
		attr.config = hwconfigs[i];

		flags = PERF_FLAG_PID_CGROUP;
		pid = cgrp_id;

		fd[i] = perf_event_open(&attr, pid, cpu, cgrp_fd, flags);
		if (fd[i] < 0) {
			int ret = errno;
			fprintf(stderr, "WARN:event %s may not supported\n", events_str[i]);
			if (ret == ENODEV)
				printf("cpu may OFF LINE\n");
		}
		/* group leader */
		if (i == 0)
			cgrp_fd = fd[i];
	}
	ioctl(cgrp_fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
	ioctl(cgrp_fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
	sleep(span);

	memset(hw_events_cnt, 0, sizeof(hw_events_cnt));
	for (i = 0; i < NUM_ENV; i++) {
		if (fd[i] < 0 || (long long)(hwconfigs[i]) < 0)
			continue;
		read(fd[i], &hw_events_cnt[i], sizeof(hw_events_cnt[i]));
#ifdef DEBUG
		printf("%s:%llu\n", events_str[i], hw_events_cnt[i]);
#endif
	}
	cache_miss_rate = EVENT_RATE(PERF_COUNT_HW_CACHE_MISSES, PERF_COUNT_HW_CACHE_REFERENCES);
	cpi = EVENT_RATE(PERF_COUNT_HW_CPU_CYCLES, PERF_COUNT_HW_INSTRUCTIONS);
	ins_stall_rate = EVENT_RATE(PERF_COUNT_HW_STALLED_CYCLES_FRONTEND, PERF_COUNT_HW_INSTRUCTIONS);
	printf("%s/%s : %llu/%llu  (%.6f)\n",
		ARGS_FMT(PERF_COUNT_HW_CACHE_LL_R_MISS, PERF_COUNT_HW_CACHE_LL_R_ACCE));
	printf("%s/%s (CPI): %llu/%llu  (%.6f)\n",
		ARGS_FMT(PERF_COUNT_HW_CPU_CYCLES, PERF_COUNT_HW_INSTRUCTIONS));
	printf("%s/%s : %llu/%llu  (%.6f)\n",
		ARGS_FMT(PERF_COUNT_HW_STALLED_CYCLES_FRONTEND, PERF_COUNT_HW_INSTRUCTIONS));

	if (cpi > 1)
		printf("CPI > 1,Memory may hit a bottleneck\n");
	else
		printf("CPI is ok %f\n", cpi);
}	

