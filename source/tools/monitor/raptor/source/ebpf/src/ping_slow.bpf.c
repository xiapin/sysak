#include "vmlinux/vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include "profile.bpf.h"

#define bpf_print(fmt, ...) 	\
({ 					\
	char ____fmt[] = fmt; 		\
	bpf_trace_printk(____fmt, sizeof(____fmt), 	\
			##__VA_ARGS__); 	\
}) 	

struct trace_info_t {
	__u64 last_ts;
};

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, struct profile_key_t);
	__type(value, u32);
	__uint(max_entries, PROFILE_MAPS_SIZE);
} counts SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_STACK_TRACE);
	__uint(key_size, sizeof(u32));
	__uint(value_size, PERF_MAX_STACK_DEPTH * sizeof(u64));
	__uint(max_entries, PROFILE_MAPS_SIZE);
} stacks SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
	__uint(max_entries, 1);
	__type(key, int);
	__type(value, struct trace_info_t);
} trace_map SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
    __type(key, u32);
    __type(value, struct net_args);
    __uint(max_entries, 1);
} args SEC(".maps");

#define KERN_STACKID_FLAGS (0 | BPF_F_FAST_STACK_CMP)
#define USER_STACKID_FLAGS (0 | BPF_F_FAST_STACK_CMP | BPF_F_USER_STACK)

SEC("perf_event")
int do_perf_event(struct bpf_perf_event_data *ctx)
{
    u64 id = bpf_get_current_pid_tgid();
	u32 cpu = bpf_get_smp_processor_id();
    u32 tgid = id >> 32;
    u32 pid = id;
	struct profile_key_t key = { .pid = tgid };
	u32 *val, one = 1, zero = 0;

	struct net_args *arg = bpf_map_lookup_elem(&args, &zero);
    if (!arg) {
        return 0;
    }

	u64 ts = bpf_ktime_get_ns();
	struct trace_info_t *info = bpf_map_lookup_elem(&trace_map, &zero);
	if (!info) {
		struct trace_info_t val = {
			.last_ts = ts,
		};
		bpf_map_update_elem(&trace_map, &zero, &val, 0);
		return 0;
	}
	// value is ok
	s64 delay = ts - info->last_ts;
	//bpf_print("delay:%d, delta:%d\n", arg->delay, delay);
	if (delay > arg->delay * 1000000 && pid != 0) {
		bpf_get_current_comm(&key.comm, sizeof(key.comm));

		key.kern_stack = bpf_get_stackid(ctx, &stacks, KERN_STACKID_FLAGS);
		key.user_stack = bpf_get_stackid(ctx, &stacks, USER_STACKID_FLAGS);

		val = bpf_map_lookup_elem(&counts, &key);
		if (val)
			(*val)++;
		else
			bpf_map_update_elem(&counts, &key, &one, BPF_NOEXIST);

		//bpf_print("ping_slow, delay:%d\n", delay);
	}
	info->last_ts = ts;
    return 0;
}

char _license[] SEC("license") = "GPL"; //todo
