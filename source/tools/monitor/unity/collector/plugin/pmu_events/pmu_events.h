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

#define	NR_GROUP	3
#define NR_EVENTS	6
#define NR_CELL	2

__u32 hw_types[] = {
	PERF_TYPE_HARDWARE,
	PERF_TYPE_HARDWARE,
	PERF_TYPE_HW_CACHE,
	PERF_TYPE_HW_CACHE,
	PERF_TYPE_HW_CACHE,
	PERF_TYPE_HW_CACHE,
};

__u64 hw_configs[] = {
	PERF_COUNT_HW_CPU_CYCLES,
	PERF_COUNT_HW_INSTRUCTIONS,
	PERF_COUNT_HW_CACHE_LL			<<  0  |
	(PERF_COUNT_HW_CACHE_OP_READ		<<  8) |
	(PERF_COUNT_HW_CACHE_RESULT_ACCESS	<< 16),
	PERF_COUNT_HW_CACHE_LL			<<  0  |
	(PERF_COUNT_HW_CACHE_OP_READ		<<  8) |
	(PERF_COUNT_HW_CACHE_RESULT_MISS	<< 16),
	PERF_COUNT_HW_CACHE_LL			<<  0  |
	(PERF_COUNT_HW_CACHE_OP_WRITE		<<  8) |
	(PERF_COUNT_HW_CACHE_RESULT_ACCESS	<< 16),
	PERF_COUNT_HW_CACHE_LL			<<  0  |
	(PERF_COUNT_HW_CACHE_OP_WRITE		<<  8) |
	(PERF_COUNT_HW_CACHE_RESULT_MISS	<< 16),
};

enum {
	CYCLES,
	INSTRUCTIONS,
	LLC_LOAD_REF,
	LLC_LOAD_MISS,
	LLC_STORE_REF,
	LLC_STORE_MISS,
};

struct hw_info {
	int fd, leader;
	__u32 type;
	__u64 config;
	__u64 count, prv_cnt;
};

struct pcpu_hw_info {
	pid_t pid;
	int cpu;
	unsigned long flags;
	struct hw_info hwi[NR_EVENTS];
};

int init(void * arg);
int call(int t, struct unity_lines* lines);
void deinit(void);

#endif //UNITY_PMU_EVENT_H
