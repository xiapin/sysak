#ifndef UNITY_PMU_EVENT_H
#define UNITY_PMU_EVENT_H
#include <stdio.h>
#include <stdlib.h>
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
#include "../plugin_head.h"

#define NR_EVENTS	2
__u64 hwconfigs[] = {
	PERF_COUNT_HW_CPU_CYCLES,
	PERF_COUNT_HW_INSTRUCTIONS,
};

enum {
	PERF_COUNT_HW_CACHE_LL_R_ACCE = PERF_COUNT_HW_MAX,
	PERF_COUNT_HW_CACHE_LL_R_MISS,
};

struct hw_info {
	int fd;
	__u32 type;
	__u64 config;
	__u64 count, prv_cnt;
};

struct pcpu_hw_info {
	pid_t pid;
	unsigned long flags;
	int cpu, fd_leader;
	struct hw_info hw[NR_EVENTS];
};

int init(void * arg);
int call(int t, struct unity_lines* lines);
void deinit(void);

#endif //UNITY_PMU_EVENT_H
