#include "vmlinux/vmlinux.h"
#include "bpf/bpf_helpers.h"
#include "bpf/bpf_tracing.h"
#include "profile.bpf.h"

#define bpf_print(fmt, ...) 	\
({ 					\
	char ____fmt[] = fmt; 		\
	bpf_trace_printk(____fmt, sizeof(____fmt), 	\
			##__VA_ARGS__); 	\
}) 	

struct space_rx_args {
	u64 pad;
	void * skaddr;
	u16 sport;
	u16 dport;
	u8 saddr[4];
	u8 daddr[4];
};

struct probe_args {
	u64 pad;
	u8 saddr[28];
	u8 daddr[28];
	u16 sport;
	u16 dport;
	u32 mark;
	u16 data_len;
	u32 snd_nxt;
	u32 snd_una;
	u32 snd_cwnd;
	u32 ssthresh;
	u32 snd_wnd;
	u32 srtt;
	u32 rcv_wnd;
	u64 sock_cookie;
};

/*
struct {
	__uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
	__type(key, u32);
	__type(value, u32);
} perf_map SEC(".maps");
*/

struct {
	__uint(type, BPF_MAP_TYPE_STACK_TRACE);
	__uint(key_size, sizeof(u32));
	__uint(value_size, PERF_MAX_STACK_DEPTH * sizeof(u64));
	__uint(max_entries, PROFILE_MAPS_SIZE);
} stacks SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 1);
	__type(key, int);
	__type(value, struct trace_info);
} trace_map SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 1);
	__type(key, int);
	__type(value, struct net_args);
} args SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, struct profile_key_t);
	__type(value, u32);
	__uint(max_entries, PROFILE_MAPS_SIZE);
} counts SEC(".maps");

#define KERN_STACKID_FLAGS (0 | BPF_F_FAST_STACK_CMP)
#define USER_STACKID_FLAGS (0 | BPF_F_FAST_STACK_CMP | BPF_F_USER_STACK)
/*
#define OFFSET_OF(type, member) (unsigned long)(&(((type*)0)->member))
#define SKB_OFFSET_HEAD OFFSET_OF(struct sk_buff, head)
#define SKB_OFFSET_NETWORK_HEADER OFFSET_OF(struct sk_buff, network_header)

#define ntohs(x) (u16)__builtin_bswap16((u16)x)
#define ntohl(x) (u32)__builtin_bswap32((u16)x)

#define fib_bpf_printk(fmt, ...) 			\
({ 							\
	char ____fmt[] = fmt; 				\
	bpf_trace_printk(____fmt, sizeof(____fmt), 	\
			##__VA_ARGS__); 		\
}) 							\
*/

__attribute__((always_inline)) static inline struct trace_info * get_trace_info(void *map, int key) {
	struct trace_info *ret;

	ret = bpf_map_lookup_elem(map, &key);
	if (!ret) {
		return NULL;
	}
	return ret;
}

__attribute__((always_inline)) static void set_trace_info(void *map, int key, struct probe_args *probe) {

	struct trace_info info = {0};
	struct trace_info *info_p = bpf_map_lookup_elem(&trace_map, &key);
	if (info_p == NULL) {
		info.ts = bpf_ktime_get_ns();
		info.cpu1 = bpf_get_smp_processor_id();
		info.ref = 1;
		bpf_map_update_elem(map, &key, &info, 0);
	}
}

SEC("tracepoint/tcp/tcp_probe")
int tcp_probe_hook(struct probe_args *probe_args)
{
	u64 ts = bpf_ktime_get_ns();
	int key = 0;
	//bpf_print("tcp_probe\n");
	struct net_args *trace_args = bpf_map_lookup_elem(&args, &key);
	if (!trace_args)
		return 0;

	if (probe_args->dport == trace_args->dport)
		set_trace_info(&trace_map, 0, probe_args);	

	return 0;
}

SEC("tracepoint/tcp/tcp_rcv_space_adjust")
int tcp_space_adjust_hook(struct space_rx_args *rx_args)
{
	struct trace_info *info;
	struct trace_info old_info = {0};
	int key = 0;

	//bpf_print("tcp_rcv_space\n");
	struct net_args *user_args = bpf_map_lookup_elem(&args, &key);
	if (!user_args)
		return 0;

	if (rx_args->dport == user_args->dport) {
		bpf_map_delete_elem(&trace_map, &key);
	}

	return 0;
}

SEC("perf_event")
int do_perf_event(struct bpf_perf_event_data *ctx)
{
	int key = 0;
	u32 one = 1;
	struct trace_info old_info = {0};
	struct trace_info *info = get_trace_info(&trace_map, 0);
	if (!info)
		return 0;

	//bpf_print("userslow profiling\n");
	old_info = *info;
	u64 ts = bpf_ktime_get_ns();
	struct net_args *user_agrs = bpf_map_lookup_elem(&args, &key);
	if (!user_agrs)	
		return 0;

	// delay for ms
	if (ts - old_info.ts > user_agrs->delay * 1000000) {
		/*
		old_info.cpu2 = bpf_get_smp_processor_id();
		old_info.kern_stack = bpf_get_stackid(ctx, &stacks, KERN_STACKID_FLAGS);
		old_info.user_stack = bpf_get_stackid(ctx, &stacks, USER_STACKID_FLAGS);
		bpf_get_current_comm(&old_info.comm, sizeof(old_info.comm));
		bpf_perf_event_output(ctx, &perf_map, BPF_F_CURRENT_CPU, &old_info,
					sizeof(struct trace_info));
		*/
		struct profile_key_t key = {0};
		bpf_get_current_comm(&key.comm, sizeof(key.comm));

		key.pid = bpf_get_current_pid_tgid();
		key.kern_stack = bpf_get_stackid(ctx, &stacks, KERN_STACKID_FLAGS);
		key.user_stack = bpf_get_stackid(ctx, &stacks, USER_STACKID_FLAGS);

		u32 *val = bpf_map_lookup_elem(&counts, &key);
		if (val)
			(*val)++;
		else
			bpf_map_update_elem(&counts, &key, &one, BPF_NOEXIST);
	}

	return 0;
}

char LICENSE[] SEC("license") = "GPL";
