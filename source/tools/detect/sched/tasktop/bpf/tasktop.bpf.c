#include <vmlinux.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <coolbpf.h>

#include "../common.h"

#define TASK_UNINTERRUPTIBLE 2
#define PF_KTHREAD 0x00200000

#define _(P)                                   \
    ({                                         \
        typeof(P) val = 0;                     \
        bpf_probe_read(&val, sizeof(val), &P); \
        val;                                   \
    })

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, u32);
    __type(value, struct arg_to_bpf);
} arg_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 4096);
    __type(key, u32);
    __type(value, struct proc_fork_info_t);
} fork_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, 1);
    __type(key, u32);
    __type(value, u64);
} cnt_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 10240);
    __type(key, struct d_task_key_t);
    __type(value, struct d_task_info_t);
} d_task_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 10240);
    __type(key, u32);
    __type(value, u64);
} start_query_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
    __uint(key_size, sizeof(int));
    __uint(value_size, sizeof(u32));
} d_task_notify_map SEC(".maps");

struct trace_event_sched_process_fork_args {
    struct trace_entry ent;
    char parent_comm[16];
    pid_t parent_pid;
    char child_comm[16];
    pid_t child_pid;
};

struct trace_event_sched_switch_args {
    struct trace_entry ent;
    char prev_comm[16];
    pid_t prev_pid;
    int prev_prio;
    long prev_state;
    char next_comm[16];
    pid_t next_pid;
    int next_prio;
};

struct trace_event_sched_stat_blocked_args {
    struct trace_entry ent;
    char comm[16];
    pid_t pid;
    u64 delay;
};

/* for eBPF-CORE process different kernel version */
struct task_struct___old {
    long state;
} __attribute__((preserve_access_index));

struct task_struct_mock {
    struct thread_info info;
    long state;
};

static __always_inline void update_cnt_map() {
    u32 zero = 0;
    u64 *value = 0;

    value = bpf_map_lookup_elem(&cnt_map, &zero);
    if (value) {
        (*value)++;
    }
}

static __always_inline void update_fork_map() {
    u32 ppid, pid, zero = 0;
    struct task_struct *task;
    struct proc_fork_info_t *prev_info;
    struct proc_fork_info_t new_info;

    task = (void *)bpf_get_current_task();
    pid = _(task->pid);
    ppid = BPF_CORE_READ(task, parent, pid);

    prev_info = bpf_map_lookup_elem(&fork_map, &pid);
    if (prev_info) {
        prev_info->fork++;
    } else {
        __builtin_memset(&new_info, 0, sizeof(struct proc_fork_info_t));
        new_info.fork = 1;
        new_info.pid = pid;
        new_info.ppid = ppid;
        bpf_get_current_comm(&new_info.comm, sizeof(new_info.comm));
        bpf_map_update_elem(&fork_map, &pid, &new_info, BPF_ANY);
    }
}

static __always_inline int fork_enable() {
    u32 key = 0;
    struct arg_to_bpf *args = bpf_map_lookup_elem(&arg_map, &key);
    if (!args) {
        return 0;
    }
    return args->fork_enable;
}

SEC("tp/sched/sched_process_fork")
int handle__sched_process_fork(
    struct trace_event_sched_process_fork_args *ctx) {
    if (fork_enable() == 0) {
        return 0;
    }

    update_cnt_map();
    update_fork_map();
    return 0;
}

static __always_inline void insert_one_task(u32 pid, char *comm, int verbose) {
    struct d_task_key_t key;
    struct d_task_info_t val;
    __builtin_memset(&key, 0, sizeof(struct d_task_key_t));
    __builtin_memset(&val, 0, sizeof(struct d_task_info_t));
    int err = 0;
    u64 now = 0;

    now = bpf_ktime_get_ns();
    if (!comm) {
        return;
    }

    err = bpf_map_update_elem(&start_query_map, &pid, &now, BPF_NOEXIST);
    if (err) {
        char fmt[] =
            "error[insert_one_task]: update start_query_map error. err=(%d) "
            "pid=(%d)\n";
        bpf_trace_printk(fmt, sizeof(fmt), err, pid);
    }

    key.pid = pid;
    key.start_time_ns = now;
    val.is_recorded = 0;
    bpf_probe_read(val.comm, 16, comm);

    err = bpf_map_update_elem(&d_task_map, &key, &val, BPF_NOEXIST);
    if (err) {
        char fmt[] =
            "error[insert_one_task]: update d_task_map error. err=(%d) "
            "pid=(%d)\n";
        bpf_trace_printk(fmt, sizeof(fmt), err, pid);
    }

    if (verbose) {
        char fmt[] =
            "debug[insert_one_task]: success record start time "
            "pid=(%d)\n";
        bpf_trace_printk(fmt, sizeof(fmt), pid);
    }
}

static void __always_inline delete_one_task(u32 pid, u64 threshold, void *ctx,
                                            int verbose) {
    struct d_task_key_t key;
    struct d_task_blocked_event_t ev;
    struct d_task_info_t *val = 0;
    u64 *start_ns = 0;
    u64 now = bpf_ktime_get_ns();
    u64 duration_ns = 0;
    int err = 0;

    __builtin_memset(&key, 0, sizeof(struct d_task_key_t));
    __builtin_memset(&ev, 0, sizeof(struct d_task_blocked_event_t));

    key.pid = pid;
    start_ns = (u64 *)bpf_map_lookup_elem(&start_query_map, &pid);
    if (!start_ns) {
        return;
    }
    key.start_time_ns = *start_ns;

    val = bpf_map_lookup_elem(&d_task_map, &key);
    if (!val) {
        char fmt[] =
            "error[delete_one_task]:  query bpf_map_lookup_elem error. "
            "err=(%d) "
            "pid=(%d)\n";
        bpf_trace_printk(fmt, sizeof(fmt), err, pid);
        return;
    }

    duration_ns = now - key.start_time_ns;
    if (duration_ns >= threshold || val->is_recorded != 0) {
        ev.duration_ns = duration_ns;
        ev.pid = pid;
        ev.start_time_ns = key.start_time_ns;
        bpf_probe_read(ev.info.comm, 16, val->comm);

        err = bpf_perf_event_output(ctx, &d_task_notify_map, BPF_F_CURRENT_CPU,
                                    &ev, sizeof(ev));
        if (err) {
            char fmt[] =
                "error[send_event]: bpf_perf_event_output error. err=(%d) "
                "pid=(%lu)\n";
            bpf_trace_printk(fmt, sizeof(fmt), err, pid);
            return;
        }
    }

    err = bpf_map_delete_elem(&start_query_map, &pid);
    if (err) {
        char fmt[] =
            "error[delete_one_task]:  delete start_query_map error. err=(%d) "
            "pid=(%d)\n";
        bpf_trace_printk(fmt, sizeof(fmt), err, pid);
        return;
    }

    err = bpf_map_delete_elem(&d_task_map, &key);
    if (err) {
        char fmt[] =
            "error[delete_one_task]:  delete d_task_map error. err=(%d) "
            "pid=(%d)\n";
        bpf_trace_printk(fmt, sizeof(fmt), err, pid);
        return;
    }

    if (verbose) {
        char fmt[] =
            "debug[delete_one_task]: success delete record "
            "pid=(%d)\n";
        bpf_trace_printk(fmt, sizeof(fmt), pid);
    }
}

SEC("tp/sched/sched_switch")
int handle__sched_switch(struct trace_event_sched_switch_args *ctx) {
    u32 key = 0, flags = 0;
    long state = 0;
    struct task_struct *tsk = 0;

    struct arg_to_bpf *args = bpf_map_lookup_elem(&arg_map, &key);
    if (!args || args->blocked_enable == 0) {
        return 0;
    }

    tsk = (void *)bpf_get_current_task();
    if (bpf_core_field_exists(tsk->__state)) {
        state = BPF_CORE_READ(tsk, __state);
    } else {
        struct task_struct___old *t_old = (void *)tsk;
        state = BPF_CORE_READ(t_old, state);
    }
    // state = _(tsk_mock->state);

    flags = BPF_CORE_READ(tsk, flags);
    /* if not enable kernel-thread, only record user-thread*/
    if (state & TASK_UNINTERRUPTIBLE &&
        (args->kthread_enable != 0 || ~flags & PF_KTHREAD)) {
        insert_one_task(ctx->prev_pid, ctx->prev_comm, args->verbose_enable);
    }

    delete_one_task(ctx->next_pid, args->threshold_ns, ctx,
                    args->verbose_enable);
    return 0;
}

SEC("tp/sched/sched_stat_blocked")
int handle__sched_stat_blocked(
    struct trace_event_sched_stat_blocked_args *ctx) {
    u32 key = 0;
    struct arg_to_bpf *args = bpf_map_lookup_elem(&arg_map, &key);
    if (!args || args->blocked_enable == 0) {
        return 0;
    }

    delete_one_task(ctx->pid, args->threshold_ns, ctx, args->verbose_enable);
    return 0;
}

char LICENSE[] SEC("license") = "GPL";