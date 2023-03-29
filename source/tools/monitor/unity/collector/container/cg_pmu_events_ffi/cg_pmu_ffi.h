#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <asm/unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define NR_EVENTS  7

struct static_arg {
	__u32 type;
	__u64 config;
	int group;
} static_args [] = {
#if 0
	{
		.type = PERF_TYPE_HARDWARE,
		.config = PERF_COUNT_HW_CPU_CYCLES, 
		.group = 0,
	},
	{
		.type = PERF_TYPE_HARDWARE,
		.config = PERF_COUNT_HW_INSTRUCTIONS, 
		.group = 0,
	},
	{
		.type = PERF_TYPE_HARDWARE,
		.config = PERF_COUNT_HW_REF_CPU_CYCLES, 
		.group = 0,
	},
	{
		.type = PERF_TYPE_HW_CACHE,
		.config = PERF_COUNT_HW_CACHE_LL << 0|
			(PERF_COUNT_HW_CACHE_OP_READ << 8)|
			(PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16), 
		.group = 0,
	},
	{
		.type = PERF_TYPE_HW_CACHE,
		.config = PERF_COUNT_HW_CACHE_LL << 0|
			(PERF_COUNT_HW_CACHE_OP_READ << 8)|
			(PERF_COUNT_HW_CACHE_RESULT_MISS << 16), 
		.group = 0,
	},
	{
		.type = PERF_TYPE_HW_CACHE,
		.config = PERF_COUNT_HW_CACHE_LL << 0|
			(PERF_COUNT_HW_CACHE_OP_WRITE << 8)|
			(PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16), 
		.group = 0,
	},
	{
		.type = PERF_TYPE_HW_CACHE,
		.config = PERF_COUNT_HW_CACHE_LL << 0|
			(PERF_COUNT_HW_CACHE_OP_WRITE << 8)|
			(PERF_COUNT_HW_CACHE_RESULT_MISS << 16), 
		.group = 0,
	},
#endif
	{
		.type = PERF_TYPE_SOFTWARE,
		.config = PERF_COUNT_SW_CPU_CLOCK,
	},
	{
		.type = PERF_TYPE_SOFTWARE,
		.config = PERF_COUNT_SW_CONTEXT_SWITCHES,
	},
	{
		.type = PERF_TYPE_SOFTWARE,
		.config = PERF_COUNT_SW_TASK_CLOCK,
	},
	{
		.type = PERF_TYPE_SOFTWARE,
		.config = PERF_COUNT_SW_PAGE_FAULTS,
	},
	{
		.type = PERF_TYPE_SOFTWARE,
		.config = PERF_COUNT_SW_PAGE_FAULTS_MIN,
	},
	{
		.type = PERF_TYPE_SOFTWARE,
		.config = PERF_COUNT_SW_CPU_MIGRATIONS,
	},
	{
		.type = PERF_TYPE_SOFTWARE,
		.config = PERF_COUNT_SW_PAGE_FAULTS_MAJ,
	},
};

struct dyn_arg {
	int pid, cpu;
	unsigned long flags;
};

struct event_info {
	int fd;
	unsigned long long prev, cnt, delta;
};

typedef struct pcpu_hwi {
	struct event_info ei[NR_EVENTS];
} pcpu_hwi_t;

