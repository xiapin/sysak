#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include <linux/version.h>
#include "../process_tree.h"

#define NULL ((void *)0)

#define _(P) ({typeof(P) val = 0; bpf_probe_read((void*)&val, sizeof(val), (const void*)&P); val; })

struct bpf_map_def SEC("maps") args_event = {
    .type = BPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(u32),
    .value_size = sizeof(struct args),
    .max_entries = 1,
};

struct bpf_map_def SEC("maps") e_process_chain = {
    .type = BPF_MAP_TYPE_PERF_EVENT_ARRAY,
    .key_size = sizeof(u32),
    .value_size = sizeof(u32),
    .max_entries = 100,
};

struct bpf_map_def SEC("maps") pid_map_event = {
    .type = BPF_MAP_TYPE_HASH,
    .key_size = sizeof(u32),
    .value_size = sizeof(u32),
    .max_entries = 50000,
};

static __always_inline struct pid *task_pid(struct task_struct *tsk)
{
    struct pid *p;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0) && !defined(RHEL_RELEASE_GT_8_0))
    p = (BPF_CORE_READ(tsk, pids)[PIDTYPE_PID]).pid;
#else
    p = BPF_CORE_READ(tsk, thread_pid);
#endif
    return p;
    // return BPF_CORE_READ(tsk, thread_pid);
}

static __always_inline pid_t get_root_task_pid(struct task_struct *tsk)
{
    return (BPF_CORE_READ(task_pid(tsk), numbers)[0]).nr;
}

static __always_inline int save_args_str_arr_to_buf(char *buf, const char *start, const char *end)
{
    if (start >= end)
    {
        return 0;
    }

    // Limit max len
    u32 len = end - start;
    if (len > MAX_ARR_LEN - 1)
    {
        len = MAX_ARR_LEN - 1;
    }

    if (bpf_probe_read(buf, len & (MAX_ARR_LEN - 1), start) == 0)
    {
        return 1;
    }
    return 0;
}

static __always_inline int save_args_str_arr_to_buf_from_task_struct(char *buf, struct task_struct *tsk)
{
    struct mm_struct *mm = _(tsk->mm);
    char *arg_start = (char *)_(mm->arg_start);
    char *arg_end = (char *)_(mm->arg_end);
    save_args_str_arr_to_buf(buf, arg_start, arg_end);
    return 0;
}

static __always_inline pid_t get_args_pid()
{
    int key = 0;
    struct args *args;
    args = bpf_map_lookup_elem(&args_event, &key);
    if (!args)
    {
        return 1;
    }
    return args->pid;
}

static __always_inline bool is_exists(pid_t pid)
{
    u32 key = pid;
    return bpf_map_lookup_elem(&pid_map_event, &key) != NULL;
}

/**
 * 监控进程创建，跟踪进程父子关系
 */
SEC("raw_tp/sched_process_fork")
int trace_sched_process_fork(struct bpf_raw_tracepoint_args *ctx)
{
    struct task_struct *parent = (struct task_struct *)ctx->args[0];
    struct task_struct *child = (struct task_struct *)ctx->args[1];
    struct task_struct *cur, *next = NULL;

    struct process_tree_event event = {
        .type = 0,
    };

    pid_t parent_pid = get_root_task_pid(parent);

    if (is_exists(parent_pid))
    {
        // Ignore backtracking
        event.p_pid = parent_pid;
        event.c_pid = get_root_task_pid(child);
        pid_t parent_pid = get_root_task_pid(parent);
        bpf_core_read_str(&event.comm[0], TASK_COMM_LEN, &parent->comm[0]);
        save_args_str_arr_to_buf_from_task_struct(&event.args[0], parent);
        bpf_perf_event_output(ctx, &e_process_chain, BPF_F_CURRENT_CPU, &event, sizeof(event));
        return 0;
    }
    else
    {
        cur = child;
        parent = NULL;
// Do backtracking
#pragma clang loop unroll(full)
        for (int i = 0; i < MAX_DEPTH; i++)
        {
            parent = _(cur->parent);
            event.p_pid = get_root_task_pid(parent);
            bpf_core_read_str(&event.comm[0], TASK_COMM_LEN, &parent->comm[0]);
            save_args_str_arr_to_buf_from_task_struct(&event.args[0], parent);
            event.c_pid = get_root_task_pid(cur);
            bpf_perf_event_output(ctx, &e_process_chain, BPF_F_CURRENT_CPU, &event, sizeof(event));

            if (event.p_pid == 1)
            {
                break;
            }
            cur = _(cur->parent);
        }
    }
    return 0;
}

/**
 * 监控进程 comm 变更
 */
SEC("raw_tp/sched_process_exec")
int trace_shced_process_exec(struct bpf_raw_tracepoint_args *ctx)
{
    struct task_struct *tsk = (struct task_struct *)ctx->args[0];
    struct linux_binprm *bprm = (struct linux_binprm *)ctx->args[2];

    struct process_tree_event event = {
        .type = 1,
    };
    bpf_core_read_str(&event.comm[0], TASK_COMM_LEN, &tsk->comm[0]);
    save_args_str_arr_to_buf_from_task_struct(&event.args[0], tsk);
    event.c_pid = get_root_task_pid(tsk);
    bpf_perf_event_output(ctx, &e_process_chain, BPF_F_CURRENT_CPU, &event, sizeof(event));
}

/**
 * 监控进程退出
 */
SEC("raw_tp/sched_process_exit")
int trace_sched_process_exit(struct bpf_raw_tracepoint_args *ctx)
{
    struct task_struct *tsk = (struct task_struct *)ctx->args[0];
    struct process_tree_event event = {
        .type = 2,
    };
    // bpf_core_read_str(&event.comm[0], TASK_COMM_LEN, &tsk->comm[0]);
    // save_args_str_arr_to_buf_from_task_struct(&event.args[0], tsk);
    event.c_pid = get_root_task_pid(tsk);
    bpf_perf_event_output(ctx, &e_process_chain, BPF_F_CURRENT_CPU, &event, sizeof(event));
}

char _license[] SEC("license") = "GPL";