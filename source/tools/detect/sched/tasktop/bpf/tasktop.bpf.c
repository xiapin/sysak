#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include <coolbpf.h>

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 1);
    __type(key, u32);
    __type(value, u64);
} fork SEC(".maps");

// struct bpf_map_def SEC("maps") fork_counter = {
//     .type = BPF_MAP_TYPE_HASH,
//     .key_size = sizeof(int),
//     .value_size = sizeof(int),
//     .max_entries = 1,
// };

struct trace_event_sys_enter_vfork_args {
    struct trace_entry ent;
};

struct trace_event_sys_enter_fork_args {
    struct trace_entry ent;
};

struct trace_event_sys_enter_clone_args {
    struct trace_entry ent;
};

struct trace_event_sched_process_fork_args {
    struct trace_entry ent;
};

static void record() {
    u32 key = 0;
    u64 val = 1;
    u64 *value;

    value = bpf_map_lookup_elem(&fork, &key);
    if (value) {
        __sync_fetch_and_add(value, 1);
    } else {
        bpf_map_update_elem(&fork, &key, &val, BPF_ANY);
    }
}

SEC("tp/syscalls/sys_enter_vfork")
int handle__sys_enter_vfork(struct trace_event_sys_enter_vfork_args *ctx) {
    record();
    return 0;
}

SEC("tp/syscalls/sys_enter_fork")
int handle__sys_enter_fork(struct trace_event_sys_enter_fork_args *ctx) {
    record();
    return 0;
}

// SEC("tp/syscalls/sys_enter_clone")
// int handle__sys_enter_clone(struct trace_event_sys_enter_fork_args *ctx) {
//     record();
//     return 0;
// }

// SEC("tp/sched/sched_process_fork")
// int handle__sched_process_fork(struct trace_event_sched_process_fork_args *ctx) {
//     record();
//     return 0;
// }

char LICENSE[] SEC("license") = "GPL";
