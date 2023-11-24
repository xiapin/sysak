#ifndef TASKTOP_COMMON_H
#define TASKTOP_COMMON_H

typedef u_int64_t u64;
typedef u_int32_t u32;
typedef int32_t s32;
typedef int64_t s64;

struct proc_fork_info_t {
    u32 pid;
    u32 ppid;
    u64 fork;
    char comm[32];
};

struct d_task_info_t {
    char comm[32];
    s32 is_recorded;
};

struct d_task_blocked_event_t {
    struct d_task_info_t info;
    u32 pid;
    u64 start_time_ns;
    u64 duration_ns;
};

struct arg_to_bpf {
    u64 threshold_ns;
    s32 fork_enable;
    s32 blocked_enable;
    s32 verbose_enable;
    s32 kthread_enable;
};

struct d_task_key_t {
    u32 pid;
    u64 start_time_ns;
};

#endif