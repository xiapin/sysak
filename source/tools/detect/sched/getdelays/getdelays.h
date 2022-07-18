#ifndef __RUNQSLOWER_H
#define __RUNQSLOWER_H
#include <asm-generic/unistd.h>
#define TASK_COMM_LEN 16
#define CPU_ARRY_LEN	4
#define CONID_LEN	13
#define MAX_NR 1024


struct event {
	char task[TASK_COMM_LEN];
	char prev_task[TASK_COMM_LEN];
	
	__u64 delay;
	__u64 stamp;
	pid_t pid;
	pid_t prev_pid;
	int cpuid;
};

struct irq_acct {
	__u64 cnt, delay_max;
	__u64 delay, stamp;
	pid_t pid, pad;
};

/*
 *@cnt:  how many times this syscall being called
 *@delay: the total times spent on this syscall
 *@delay_max: the max delay on this syscall
 *@tm_enter : the enter timestamp when max-delay happend
 *@tm_exit  : the exit timestamp when max-delay happend
 * */
struct syscalls_acct {
	__u64 cnt, delay, sys;
	__u64 delay_max, max_enter, max_exit, tm_enter, tm_exit;
	__u64 csw_begin;
	__u32 prev_state;
	__u64 sleep, wait;
	__u64 nr_sleep, nr_wait;
};

struct sys_ctx {
	__u32 stat;
	union {
		__u32 sysid;
	};
};

struct syscall_key {
	__u32 pid;
	__u32 sysid;
};

struct args {
	__u64 threshold;
	pid_t targ_pid;
	pid_t filter_pid;
	int type;
	pid_t pad;
};

#endif /* __RUNQSLOWER_H */
