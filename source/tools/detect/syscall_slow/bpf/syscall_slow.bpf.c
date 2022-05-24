#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>
#include "../syscall_slow.h"

#define TASK_RUNNING	0
#define PERF_MAX_STACK_DEPTH	127
#define KERN_STACKID_FLAGS	(0 | BPF_F_FAST_STACK_CMP)
#define _(P) ({						\
	typeof(P) val;					\
	__builtin_memset(&val, 0, sizeof(val));		\
	bpf_probe_read(&val, sizeof(val), &P);		\
	val;						\
})

#define name_n_equal(src, dst, dstn, MAXn) ({		\
	bool ret = true;				\
	int i;						\
	for (int i = 0; i < MAXn; i++) {		\
		if (i >= dstn)				\
			break;				\
		if (src[i] != dst[i]) {			\
			ret = false;			\
			break;}				\
	}						\
	ret;						\
})

/* 
 * struct trace_event_raw_sys_enter/exit may not defined in
 * some old versions, so we do a work-around 
 * */
struct raw_sys_enter_arg {
        struct trace_entry ent;
        long int id;
        long unsigned int args[6];
        char __data[0];
};

struct raw_sys_exit_arg {
	struct trace_entry ent;
	long int id;
	long int ret;
	char __data[0];
};

struct sched_sched_switch_args {
	struct trace_entry ent;
	char prev_comm[16];
	pid_t prev_pid;
	int prev_prio;
	long int prev_state;
	char next_comm[16];
	pid_t next_pid;
	int next_prio;
	char __data[0];
};

struct enter_info {
	u64 csw_begin, enter_begin;
	long int prev_state;
	u64 itime, vtime;
	u64 stime, exec_time, nvcsw, nivcsw;
	u32 ret;
};

struct bpf_map_def SEC("maps") stackmap = {
	.type = BPF_MAP_TYPE_STACK_TRACE,
	.key_size = sizeof(u32),
	.value_size = PERF_MAX_STACK_DEPTH * sizeof(u64),
	.max_entries = 10240,
};
struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 2);
	__type(key, u32);
	__type(value, struct arg_info);
} arg_map SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 10240);
	__type(key, u32);
	__type(value, struct enter_info);
} enter_map SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
	__uint(key_size, sizeof(u32));
	__uint(value_size, sizeof(u32));
} events SEC(".maps");


SEC("tp/sched/sched_switch")
int handle__sched_switch(struct sched_sched_switch_args *ctx)
{
	u64 pid, prev_pid;
	u64 now, itime, vtime, csw_begin;
	struct enter_info *enterp;

	__builtin_memset(&itime, 0,sizeof(u64));
	__builtin_memset(&vtime, 0,sizeof(u64));
	__builtin_memset(&csw_begin, 0,sizeof(u64));
	__builtin_memset(&now, 0,sizeof(u64));
	prev_pid = ctx->prev_pid;
	pid = ctx->next_pid;
	enterp = bpf_map_lookup_elem(&enter_map, &prev_pid);
	now = bpf_ktime_get_ns();
	if (enterp) {
		enterp->csw_begin = now;
		enterp->prev_state = ctx->prev_state;
		enterp->ret = bpf_get_stackid(ctx, &stackmap, KERN_STACKID_FLAGS);
	}

	enterp = bpf_map_lookup_elem(&enter_map, &pid);
	if (enterp) {
		itime = _(enterp->itime);
		vtime = _(enterp->vtime);
		csw_begin = _(enterp->csw_begin);
		if (enterp->prev_state == TASK_RUNNING) {
			enterp->itime = itime + (now - csw_begin);
		} else {
			enterp->vtime = vtime + (now - csw_begin);
		}
	}

	return 0;
}

SEC("tp/raw_syscalls/sys_enter")
int handle_raw_sys_enter(struct raw_sys_enter_arg *ctx)
{
	u64 now;
	int pid, i = 0;
	bool match;
	char comm[16] = {0};
	struct task_struct *task;
	struct arg_info *argp;
	struct enter_info *enterp, enter;
	struct filter filter;

	__builtin_memset(&filter, 0, sizeof(filter));
	bpf_get_current_comm(&comm, sizeof(comm));
	argp = bpf_map_lookup_elem(&arg_map, &i);
	if (argp)
		filter = _(argp->filter);
	else 
		return 0;
	if (filter.size && filter.size != -1) {
		unsigned long size = filter.size;
		match = name_n_equal(comm, filter.comm, size, 16);
		if (!match)
			return 0;
	} else if (filter.size == -1) {
		;/*todo*/
	} else {
		return 0;	/* size==0, skip */
	}
	pid = bpf_get_current_pid_tgid();
	task = (void *)bpf_get_current_task();
	enterp = bpf_map_lookup_elem(&enter_map, &pid);
	if (!enterp) {
		__builtin_memset(&enter, 0, sizeof(enter));
		now = bpf_ktime_get_ns();
		enter.enter_begin = now;
		enter.exec_time = BPF_CORE_READ(task, se.sum_exec_runtime);
		enter.stime = _(task->stime);
		enter.nvcsw = _(task->nvcsw);
		enter.nivcsw = _(task->nivcsw);
		enter.itime = 0;
		enter.vtime = 0;
		bpf_map_update_elem(&enter_map, &pid, &enter, 0);
	}
	return 0;
}

SEC("tp/raw_syscalls/sys_exit")
int handle_raw_sys_exit(struct raw_sys_exit_arg *ctx)
{
	int pid, i = 0;
	bool match;
	char comm[16] = {0};
	struct arg_info *argp;
	struct task_struct *task;
	struct enter_info *enterp;
	struct filter filter;
	struct event event = {};

	__builtin_memset(&filter, 0, sizeof(filter));
	bpf_get_current_comm(&comm, sizeof(comm));
	argp = bpf_map_lookup_elem(&arg_map, &i);
	if (argp)
		filter = _(argp->filter);
	else 
		return 0;
	if (filter.size && filter.size != -1) {
		unsigned long size = filter.size;
		match = name_n_equal(comm, filter.comm, size, 16);
		if (!match)
			return 0;
	} else if (filter.size == -1) {
		;/* todo: shell we monitor all threads?  */
	} else {
		return 0;	/* size==0, we skip because its too much... */
	}

	pid = bpf_get_current_pid_tgid();
	task = (void *)bpf_get_current_task();
	enterp = bpf_map_lookup_elem(&enter_map, &pid);
	if (enterp) {
		u64 delta, now, exec_time;
		now = bpf_ktime_get_ns();
		delta = now - enterp->enter_begin;
		if (delta > argp->thresh) {
			exec_time = BPF_CORE_READ(task, se.sum_exec_runtime);
			event.realtime = exec_time - enterp->exec_time;
			event.stime = _(task->stime);
			event.stime -= enterp->stime;
			event.nvcsw = _(task->nvcsw);
			event.nvcsw -= enterp->nvcsw;
			event.nivcsw = _(task->nivcsw);
			event.nivcsw -= enterp->nivcsw;
			event.itime = enterp->itime;
			event.vtime = enterp->vtime;
			event.delay = delta;
			event.sysid = ctx->id;
			event.ret = enterp->ret;
			bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU,
						&event, sizeof(event));
		}
		bpf_map_delete_elem(&enter_map, &pid);
	}
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
