#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include "../kvmexittime.h"

#define _(P) ({typeof(P) val = 0; bpf_probe_read(&val, sizeof(val), &P); val;})

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 4);
	__type(key, u32);
	__type(value, struct args);
} argmap SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 10240);
	__type(key, u32);
	__type(value, struct kvm_exit_timestamps);
} start SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 10240);
	__type(key, u32);
	__type(value, struct kvm_exit_time);
} counts SEC(".maps");

struct trace_event_raw_kvm_entry {
	struct trace_entry ent;
    unsigned int vcpu_id;
};

struct trace_event_raw_kvm_exit {
	struct trace_entry ent;
    unsigned int exit_reason;
    unsigned long guest_rip;
    u32 isa;
    u64 info1;
    u64 info2;
};

/* filter with tgid or pid */
static __always_inline
bool filter(u32 tgid, u32 pid)
{
	u64 i = 0;
	pid_t targ_tgid, targ_pid;
	struct args *argp;

	argp = bpf_map_lookup_elem(&argmap, &i);
	if (!argp)
		return false;

	targ_tgid = _(argp->targ_tgid);
	targ_pid = _(argp->targ_pid);

	if (targ_tgid && targ_tgid != tgid)
		return true;

	if (targ_pid && targ_pid != pid)
		return true;

	return false;
}

SEC("tp/kvm/kvm_exit") 
int handle__kvm_exit(struct trace_event_raw_kvm_exit *ctx) {
    u64 current;
    u32 pid, tgid;
    struct kvm_exit_timestamps *ts, zero = {.kvm_exit_timestamp=0, .exit_reason=0};

    current = bpf_get_current_pid_tgid();
    pid = current;
    tgid = current >> 32;
    if (filter(tgid, pid))
        return 0;

    ts = bpf_map_lookup_elem(&start, &pid);
    if (!ts) {
        bpf_map_update_elem(&start, &pid, &zero, BPF_NOEXIST);
        ts = bpf_map_lookup_elem(&start, &pid);
        if (!ts) {
            return 0;
        }
    }
    ts->kvm_exit_timestamp = bpf_ktime_get_ns();
    ts->exit_reason = ctx->exit_reason;
    ts->sched_switch_timestamp = 0;
    ts->sched_time = 0;
    return 0;
}

SEC("tp/kvm/kvm_entry")
int handle__kvm_entry(struct trace_event_raw_kvm_entry *ctx) {
    u64 current;
    u32 pid, tgid;
    struct kvm_exit_timestamps *ts;

    current = bpf_get_current_pid_tgid();
    pid = current;
    tgid = current >> 32;
    if (filter(tgid, pid))
        return 0;

    ts = bpf_map_lookup_elem(&start, &pid);
    if (ts != 0){
        unsigned int exit_reason = ts->exit_reason;
        struct kvm_exit_time zero = {.cumulative_time = 0, .cumulative_sched_time = 0, .count = 0};
        struct kvm_exit_time *tm;
        tm = bpf_map_lookup_elem(&counts, &exit_reason);
        if (!tm) {
            bpf_map_update_elem(&counts, &exit_reason, &zero, BPF_NOEXIST);
            tm = bpf_map_lookup_elem(&counts, &exit_reason);
            if (!tm) {
                return 0;
            }
        }
        tm->cumulative_time += bpf_ktime_get_ns() - ts->kvm_exit_timestamp;
        tm->cumulative_sched_time += ts->sched_time;
        tm->count += 1;
    }
    return 0;
}

SEC("tp/sched/sched_switch")
int handle__sched_switch(struct trace_event_raw_sched_switch *ctx) {
    u64 current;
    u32 pid, tgid;
	struct kvm_exit_timestamps *ts;

    current = bpf_get_current_pid_tgid();
    pid = current;
    tgid = current >> 32;
    if (filter(tgid, pid))
        return 0;

    ts = bpf_map_lookup_elem(&start, &pid);
	if (ts != 0) {
		ts->sched_switch_timestamp = bpf_ktime_get_ns();
	}

    return 0;
}

SEC("kprobe/finish_task_switch")
int BPF_KPROBE(finish_task_switch, struct task_struct *p) {
    u64 current;
    u32 pid, tgid;
    struct kvm_exit_timestamps *ts;

    current = bpf_get_current_pid_tgid();
    pid = current;
    tgid = current >> 32;
    if (filter(tgid, pid))
        return 0;

    ts = bpf_map_lookup_elem(&start, &pid);
	if (ts != 0 && ts->sched_switch_timestamp) {
		ts->sched_time += bpf_ktime_get_ns() - ts->sched_switch_timestamp;
	}

    return 0;
}

char LICENSE[] SEC("license") = "GPL";
