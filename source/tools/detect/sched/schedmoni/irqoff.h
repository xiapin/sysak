#ifndef __IRQOFF_H
#define __IRQOFF_H

#define TASK_COMM_LEN	16
#define CPU_ARRY_LEN	4
#define CONID_LEN	13

struct info {
	__u64 prev_counter;
};

struct tm_info {
	__u64 last_stamp;
};

struct arg_info {
	__u64 thresh;
};

struct max_sum {
	__u64 value;
	__u64 stamp;
	int cpu, pid;
	char comm[TASK_COMM_LEN];
};

struct summary {
	unsigned long num;
	__u64	total;
	struct max_sum max;
	int cpus[CPU_ARRY_LEN];
	int jitter[CPU_ARRY_LEN];
	char container[CPU_ARRY_LEN][CONID_LEN];
};
#endif /* __LLCSTAT_H */

