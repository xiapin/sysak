#ifndef __APPNOISE_H
#define __APPNOISE_H

#define MAX_SLOTS	11
#define MAX_ENTRIES_IRQ	256
#define MAX_ENTRIES 8192
struct args{
    pid_t pid;
	pid_t tgid;
};

struct numa_key_t {
    pid_t pid;
    char name[16];
};

struct thread_info_t{
	pid_t pid;
	char name[16];
    __u64 count;
};
struct histinfo {
	__u64 count;
	__u32 slots[MAX_SLOTS];
    __u64 total_time;
};

struct irq_info_t {
	char name[32];
    __u64 count;
    __u64 total_time;
};

struct info{
	__u64 count;
	__u64 total_time;
};

/*
    hist map;
    key-value
    0 - irq
    1 - softirq
    2 - nmi
    3 - wait
    4 - sleep
    5 - block
    6 - iowait
*/
enum{
	hist_irq = 0,
	hist_softirq = 1,
	hist_nmi = 2,
	hist_wait = 3,
	hist_sleep = 4,
	hist_block = 5,
	hist_iowait = 6,
	hist_syscall = 7,
	nr_hist = 8,
};

#endif /* __APPNOISE_H */