#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>
#include "../fanotifytrace.h"

#define _(P) ({						\
	typeof(P) val;					\
	__builtin_memset(&val, 0, sizeof(val));		\
	bpf_probe_read(&val, sizeof(val), &P);		\
	val;						\
})

struct bpf_map_def SEC("maps") args_map = {
	.type = BPF_MAP_TYPE_HASH,
	.key_size = sizeof(int),
	.value_size = sizeof(struct args),
	.max_entries = 1,
};

struct syscalls_enter_fanotifyinit_args {
	unsigned long long unused;
	int syscall_nr;
	long flags;
	long event_f_flags;
};

struct raw_sys_enter_arg {
        struct trace_entry ent;
        long int id;
        long unsigned int args[6];
        char __data[0];
};

struct {
	__uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
	__uint(key_size, sizeof(u32));
	__uint(value_size, sizeof(u32));
} fanotify_events SEC(".maps");

SEC("tp/raw_syscalls/sys_enter")
int handle_raw_sys_enter(struct raw_sys_enter_arg *ctx)
{
	u64 now;
	int pid, i = 0;
	bool match;
	char comm[TASK_COMM_LEN] = {0};
	struct task_struct *task;
	struct args *argp;
	struct task_info new1;

	argp = bpf_map_lookup_elem(&args_map, &i);
	if (ctx->id == _(argp->syscallid)){
		__builtin_memset(&new1, 0, sizeof(struct task_info));
		bpf_get_current_comm(new1.comm,TASK_COMM_LEN);
		new1.pid = bpf_get_current_pid_tgid();
		new1.time = bpf_ktime_get_ns();
		new1.syscallid = ctx->id;
			bpf_perf_event_output(ctx, &fanotify_events, BPF_F_CURRENT_CPU,
				&new1, sizeof(new1));
	}
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
