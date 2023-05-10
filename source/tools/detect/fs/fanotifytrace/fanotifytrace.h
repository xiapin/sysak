#ifndef __FANOTIFYTRACE_H
#define __FANOTIFYTRACE_H

#define TASK_COMM_LEN	16
#define MAX_DATE	128
#define FANOTIFY_INIT_ID 300
#define FANOTIFY_MARK_ID 301
#define SEC_TO_NS   (1000*1000*1000)
#define SEC_TO_MS   (1000*1000)

struct args {
	long int syscallid;
};

struct task_info {
	__u32 pid;
	char comm[16];
	__u64 time;
	long int syscallid;
	long int syscallid_args;
};

struct ksym {
	long addr;
	char *name;
};
#endif

