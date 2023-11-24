// #define DEBUG 1
#ifndef __TASKTOP_H
#define __TASKTOP_H
#include <sys/types.h>
#include "common.h"

#define STACK_CONTENT_LEN 1024
#define FILE_PATH_LEN 256
#define MAX_COMM_LEN 24
#define MAX_CGROUP_NAME_LEN 256
#define CPU_NAME_LEN 8
#define BUF_SIZE 512
#define DEBUG_LOG "./log/debug.log"
#define PIDMAX_PATH "/proc/sys/kernel/pid_max"
#define PROC_STAT_PATH "/proc/stat"
#define SCHEDSTAT_PATH "/proc/schedstat"

enum sort_type { SORT_SYSTEM, SORT_USER, SORT_CPU };

struct id_pair_t {
    pid_t pid;
    pid_t tid;
};

struct proc_stat_t {
    s32 pid;
    char comm[MAX_COMM_LEN];
    char state;
    s32 ppid;
    s32 pgrp;
    s32 session;
    s32 tty_nr;
    s32 tpgid;
    u32 flags;
    u64 minflt;
    u64 cminflt;
    u64 majflt;
    u64 cmajflt;
    u64 utime;
    u64 stime;
    s64 cutime;
    s64 cstime;
    s64 priority;
    s64 nice;
    s64 num_threads;
    s64 itrealvalue;
    u64 starttime;
};

struct task_cputime_t {
    s32 pid;
    s32 ppid;
    char comm[MAX_COMM_LEN];
    u64 stime;
    u64 utime;
    u64 starttime;
    u64 ts_ns;
};

struct sys_cputime_t {
    char cpu[CPU_NAME_LEN];
    s64 usr;
    s64 nice;
    s64 sys;
    s64 idle;
    s64 iowait;
    s64 irq;
    s64 softirq;
    s64 steal;
    s64 guest;
    s64 guest_nice;
};

typedef struct R_task_record_t {
    s32 pid;
    s32 ppid;
    char comm[MAX_COMM_LEN];
    time_t runtime;
    time_t begin_ts;
    double system_cpu_rate;
    double user_cpu_rate;
    double all_cpu_rate;
    u64 ts_ns;
} R_task_record_t;

typedef struct D_task_record_t {
    s32 pid;
    s32 tid;
    char comm[MAX_COMM_LEN];
    char stack[STACK_CONTENT_LEN];
} D_task_record_t;

typedef struct cgroup_cpu_stat_t {
    char cgroup_name[MAX_CGROUP_NAME_LEN];
    s32 nr_periods;
    s32 nr_throttled;
    u64 throttled_time;
    u64 wait_sum;
    u64 current_bw;
    s32 nr_burst;
    u64 burst_time;
    time_t last_update;
} cgroup_cpu_stat_t;

typedef struct cpu_util_t {
    double usr;
    double nice;
    double sys;
    double idle;
    double iowait;
    double irq;
    double softirq;
    double steal;
    double guest;
    double guest_nice;
} cpu_util_t;

typedef struct sys_record_t {
    /* util */
    cpu_util_t *cpu;

    /* load */
    float load1;
    s32 nr_R;
    s32 nr_D;
    s32 nr_fork;
    struct proc_fork_info_t most_fork_info;

    u64 *percpu_sched_delay;
} sys_record_t;

struct record_t {
    R_task_record_t **r_tasks;
    D_task_record_t *d_tasks;
    cgroup_cpu_stat_t *cgroups;
    sys_record_t sys;
};

#endif
