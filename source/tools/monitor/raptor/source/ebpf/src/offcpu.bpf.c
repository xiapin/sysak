#include "vmlinux.h"
#include "bpf/bpf_helpers.h"
#include "bpf/bpf_tracing.h"
#include "profile.bpf.h"

#define bpf_print(fmt, ...) 	\
({ 					\
	char ____fmt[] = fmt; 		\
	bpf_trace_printk(____fmt, sizeof(____fmt), 	\
			##__VA_ARGS__); 	\
}) 	

#define MAX_BLOCK_NS 1000000000
#define KERN_STACKID_FLAGS (0 | BPF_F_FAST_STACK_CMP)
#define USER_STACKID_FLAGS (0 | BPF_F_FAST_STACK_CMP | BPF_F_USER_STACK)

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __type(key, u32);
    __type(value, struct profile_bss_args_t);
    __uint(max_entries, 1);
} args SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_STACK_TRACE);
    __uint(key_size, sizeof(int));
    __uint(value_size, PERF_MAX_STACK_DEPTH * sizeof(__u64));
    __uint(max_entries, PROFILE_MAPS_SIZE);
} stacks SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(key_size, sizeof(__u32));
	__uint(value_size, sizeof(__u64));
    __uint(max_entries, 10000);
} starts SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, struct profile_key_t);
	__type(value, struct profile_value_t);
	__uint(max_entries, PROFILE_MAPS_SIZE);
} counts SEC(".maps");
/*
struct task_struct {
	__u32 pid;
    __u32 tgid;
}  __attribute__((preserve_access_index));
*/

struct tp_args {
    u64 pad;
    char prev_comm[16];
    pid_t prev_pid;
    int prev_prio;
    long prev_state;
    char next_comm[16];
    pid_t next_pid;
    // ...
};
/*
int BPF_PROG(sched_switch_fn, bool preempt, struct task_struct *prev,
		struct task_struct *next)
*/
SEC("tracepoint/sched/sched_switch")
int sched_switch_fn(struct tp_args *tp)
{
    __u32 pid, tgid;
    __u64 ts, *tsp;
    __u64 tgid_pid = bpf_get_current_pid_tgid();
    pid = (__u32)tgid_pid;
    tgid = tgid_pid >> 32;

    __u32 key_0 = 0;
    struct profile_bss_args_t *arg = bpf_map_lookup_elem(&args, &key_0);
    if (arg == NULL)
        return 0;

    //bpf_print("start1, tgid:%d, tgid_filter:%d\n", tgid, arg->tgid_filter);
    // idle threads ignore
    if (pid == 0)
        return 0;

    // tgid_filter default is 0
    if ((tgid == arg->tgid_filter) || (arg->tgid_filter == 0)) {
        // time task start off cpu
        ts = bpf_ktime_get_ns();
        bpf_map_update_elem(&starts, &pid, &ts, BPF_ANY);
    } else {
        return 0;
    }

    //bpf_print("offcpu start2\n");
    bpf_probe_read_kernel(&pid, sizeof(pid), &tp->next_pid);
    // if updated will be found
    tsp = bpf_map_lookup_elem(&starts, &pid);
    if (tsp == NULL)
        return 0; 

    __u64 start = *tsp;
    __u64 end = bpf_ktime_get_ns();
    //bpf_map_delete_elem(&starts, &pid);
    if ((start > end) || (end - start) > MAX_BLOCK_NS)
        return 0;

    //bpf_print("offcpu start3, pid:%d\n", pid);
    struct profile_key_t key = {};
    key.kern_stack = bpf_get_stackid(tp, &stacks, KERN_STACKID_FLAGS);
    key.user_stack = bpf_get_stackid(tp, &stacks, USER_STACKID_FLAGS);
    key.pid = pid;
    bpf_get_current_comm(&key.comm, sizeof(key.comm));

    struct profile_value_t *val = bpf_map_lookup_elem(&counts, &key);
    if (!val) {
        struct profile_value_t value = {};
        bpf_map_update_elem(&counts, &key, &value, BPF_NOEXIST);
        val = bpf_map_lookup_elem(&counts, &key);
        if (!val)
            return 0;
    }
    __sync_fetch_and_add(&val->counts, 1);
    __sync_fetch_and_add(&val->deltas, end - start);
    //bpf_print("offcpu start4\n");
    return 0;
}

char __license[] SEC("license") = "GPL";
