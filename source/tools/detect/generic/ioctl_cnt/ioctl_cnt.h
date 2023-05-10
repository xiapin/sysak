#ifndef __IRQOFF_H
#define __IRQOFF_H

#define TASK_COMM_LEN	16
#define CPU_ARRY_LEN	4
#define MAX_NR 1024

struct info {
	__u64 prev_counter;
};

struct tm_info {
	__u64 last_stamp;
};

struct filter {
	__u32 pid;
	int size;
	char comm[16];
};

struct arg_info {
	__u32 pid;
	__u64	thresh;
	struct filter filter;
};

struct event {
	__u32 ret, pid;
	long int sysid;
	__u64 delay, stamp;
	__u64 nvcsw, nivcsw, icnt, vcnt;
	__u64 realtime, stime, itime, vtime;
	char comm[16];
};

struct ksym {
	long addr;
	char *name;
};
#endif /* __LLCSTAT_H */

