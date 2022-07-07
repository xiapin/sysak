#ifndef __RUNQSLOWER_H
#define __RUNQSLOWER_H

#define wchar_t wchar_t_cjson
#include "cJSON.h"
#undef wchar_t

#define TASK_COMM_LEN 16
#ifdef __x86_64__
#define	TIF_NEED_RESCHED	3
#elif defined (__aarch64__)
#define TIF_NEED_RESCHED	1
#endif

enum {
	MOD_FILE = 0,
	MOD_STRING,
};

enum {
	RQSLW = 0,
	NOSCH,
	IRQOF,
	MAX_MOD,
};

struct comm_item {
	char comm[TASK_COMM_LEN];
	unsigned long size;
};

struct args {
	__u64 thresh;
	pid_t targ_pid;
	pid_t targ_tgid;
	struct comm_item comm_i;
	int flag;
	bool ready;
};

struct tm_info {
	__u64 last_stamp;
};

struct tharg {
	int map_fd;
	int ext_fd;
};

struct enq_info {
	union {
		unsigned int rqlen;
		__u64 pad;
	};
	__u64 ts;
};

struct jsons {
	cJSON *root, *datasources;
	cJSON *runqslw, *tms, *tms_data, *tbl, *tbl_data;
	cJSON *nosched;
	cJSON *irqoff;
};

struct summary {
	__u64 delay, cnt, max;
};

struct env {
	pid_t pid;
	pid_t tid;
	__u64 thresh;
	bool previous, mod_json;
	bool verbose;
	void *fp;
	__u64 sample_period;
	struct jsons json;
	struct summary summary[MAX_MOD];
	unsigned long span;
	struct comm_item comm;
};

struct ksym {
	long addr;
	char *name;
};

struct event {
	union {
		unsigned int rqlen;
		__u32 ret;
	};
	char task[TASK_COMM_LEN];
	char prev_task[TASK_COMM_LEN];
	
	__u64 delay, stamp, enter, exit;
	pid_t pid;
	pid_t prev_pid;
	int cpuid;
};

void stamp_to_date(__u64 stamp, char dt[], int len);
int print_stack(int fd, __u32 ret, int sikp, struct ksym *syms, void *fp, int mod);

#endif /* __RUNQSLOWER_H */
