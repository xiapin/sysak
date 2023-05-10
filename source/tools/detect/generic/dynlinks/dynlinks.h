#ifndef __IRQOFF_H
#define __IRQOFF_H

#define TASK_COMM_LEN	16
#define CPU_ARRY_LEN	4
#define CONID_LEN	13

struct info {
	__u64 prev_counter;
};

struct value {
	__u64 cnt;
};

struct tm_info {
	__u64 last_stamp;
	__u64 rq;
};

struct arg_info {
	__u64 thresh;
};

struct ksym {
	long addr;
	char *name;
};
#endif /* __IRQOFF_H */

