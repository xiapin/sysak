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
    int pid;
    char comm[MAX_COMM_LEN];
    char state;
    int ppid;
    int pgrp;
    int session;
    int tty_nr;
    int tpgid;
    unsigned int flags;
    u_int64_t minflt;
    u_int64_t cminflt;
    u_int64_t majflt;
    u_int64_t cmajflt;
    u_int64_t utime;
    u_int64_t stime;
    int64_t cutime;
    int64_t cstime;
    int64_t priority;
    int64_t nice;
    int64_t num_threads;
    int64_t itrealvalue;
    unsigned long long starttime;
};

struct task_cputime_t {
    int pid;
    int ppid;
    char comm[MAX_COMM_LEN];
    u_int64_t stime;
    u_int64_t utime;
    u_int64_t starttime;
};

struct sys_cputime_t {
    char cpu[CPU_NAME_LEN];
    long usr;
    long nice;
    long sys;
    long idle;
    long iowait;
    long irq;
    long softirq;
    long steal;
    long guest;
    long guest_nice;
};

typedef struct R_task_record_t {
    int pid;
    int ppid;
    char comm[MAX_COMM_LEN];
    time_t runtime;
    time_t begin_ts;
    double system_cpu_rate;
    double user_cpu_rate;
    double all_cpu_rate;
} R_task_record_t;

typedef struct D_task_record_t {
    int pid;
    int tid;
    char comm[MAX_COMM_LEN];
    char stack[STACK_CONTENT_LEN];
} D_task_record_t;

typedef struct cgroup_cpu_stat_t {
    char cgroup_name[MAX_CGROUP_NAME_LEN];
    int nr_periods;
    int nr_throttled;
    unsigned long long throttled_time;
    unsigned long long wait_sum;
    unsigned long long current_bw;
    int nr_burst;
    unsigned long long burst_time;
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
    int nr_R;
    int nr_D;
    int nr_fork;
    struct proc_fork_info_t most_fork_info;

    unsigned long long *percpu_sched_delay;
} sys_record_t;

struct record_t {
    R_task_record_t **r_tasks;
    D_task_record_t *d_tasks;
    cgroup_cpu_stat_t *cgroups;
    sys_record_t sys;
};

#endif
