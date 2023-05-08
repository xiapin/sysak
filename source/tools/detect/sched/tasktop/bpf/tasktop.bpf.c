#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include <coolbpf.h>

#include "../common.h"

#define MAX_PID

#define _(P)                                   \
    ({                                         \
        typeof(P) val = 0;                     \
        bpf_probe_read(&val, sizeof(val), &P); \
        val;                                   \
    })
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

struct trace_event_sched_process_fork_args {
    struct trace_entry ent;
    char parent_comm[16];
    pid_t parent_pid;
    char child_comm[16];
    pid_t child_pid;
};

static void update_cnt_map() {
    u32 zero = 0;
    u64 *value = 0;

    value = bpf_map_lookup_elem(&cnt_map, &zero);
    if (value) {
        (*value)++;
    }
}

static void update_fork_map() {
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

SEC("tp/sched/sched_process_fork")
int handle__sched_process_fork(struct trace_event_sched_process_fork_args *ctx) {
    update_cnt_map();
    update_fork_map();
    return 0;
}

char LICENSE[] SEC("license") = "GPL";
