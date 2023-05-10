#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include "sched_jit.h"
#include "../runqslower.h"

#define TASK_RUNNING	0
#define _(P) ({typeof(P) val = 0; bpf_probe_read(&val, sizeof(val), &P); val;})

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 4);
	__type(key, u32);
	__type(value, struct args);
} argmap SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_HASH);
	__uint(max_entries, 10240);
	__type(key, u32);
	__type(value, u64);
} start SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
	__uint(key_size, sizeof(u32));
	__uint(value_size, sizeof(u32));
} events SEC(".maps");

/*
 * the return value type can only be assigned to 0,
 * so it can be int ,long , long long and the unsinged version
 * */
#define GETARG_FROM_ARRYMAP(map,argp,type,member)({	\
	type retval = 0;			\
	int i = 0;				\
	argp = bpf_map_lookup_elem(&map, &i);	\
	if (argp) {				\
		retval = _(argp->member);		\
	}					\
	retval;					\
	})

struct sched_wakeup_tp_args {
	struct trace_entry ent;
	char comm[16];
	pid_t pid;
	int prio;
	int success;
	int target_cpu;
	char __data[0];
};

struct sched_switch_tp_args {
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

/* record enqueue timestamp */
static __always_inline
int trace_enqueue(u32 tgid, u32 pid)
{
	u64 ts, i = 0;
	pid_t targ_tgid, targ_pid, filter_pid;
	struct args *argp;

	argp = bpf_map_lookup_elem(&argmap, &i);
	if (!argp)
		return 0;

	filter_pid = _(argp->filter_pid);
	if (!pid || (pid == filter_pid))
		return 0;

	targ_tgid = _(argp->targ_tgid);
	targ_pid = _(argp->targ_pid);
	if (targ_tgid && targ_tgid != tgid)
		return 0;
	if (targ_pid && targ_pid != pid)
		return 0;

	ts = bpf_ktime_get_ns();
	bpf_map_update_elem(&start, &pid, &ts, 0);
	return 0;
}

SEC("kprobe/ttwu_do_wakeup")
int BPF_KPROBE(sysak_ttwu_do_wakeup, struct rq *rq, struct task_struct *p, 
		int wake_flags, struct rq_flags *rf)
{
	u64 cnt, *cntp;
	u32 pid;

	if (!program_ready())
		return 0;
	cnt = 0;
	pid = _(p->tgid);
	cntp = bpf_map_update_elem(&start, &pid, &ts, 0);
	if (cntp)
		*cntp++;
	else 
		bpf_map_update_elem(&start, &pid, &cnt, 0);
}

SEC("raw_tracepoint/sched_wakeup")
int sysak_rawtp_sched_wakeup(struct sched_wakeup_tp_args *ctx)
{
	unsigned int runqlen = 0;
	struct task_struct *p = (void *)ctx->args[0];
 
	runqlen = get_task_rqlen(p);
	return trace_enqueue(p, runqlen);
}

SEC("tp/sched/sched_wakeup_new")
int handle__sched_wakeup_new(struct sched_wakeup_tp_args *ctx)
{
	pid_t pid = 0;
	bpf_probe_read(&pid, sizeof(pid), &(ctx->pid));

	return trace_enqueue(0, pid);
}

SEC("tp/sched/sched_switch")
int handle_switch(struct sched_switch_tp_args *ctx)
{
	int cpuid;
	u32 pid, prev_pid;
	long int prev_state;
	struct rqevent event = {};
	u64 *tsp, delta, threshold, now;
	struct args *argp;

	prev_pid = ctx->prev_pid;
	pid = ctx->next_pid;
	prev_state = ctx->prev_state;
	cpuid = bpf_get_smp_processor_id();
	/* ivcsw: treat like an enqueue event and store timestamp */
	if (prev_state == TASK_RUNNING)
		trace_enqueue(0, prev_pid);

	/* fetch timestamp and calculate delta */
	tsp = bpf_map_lookup_elem(&start, &pid);
	if (!tsp)
		return 0;   /* missed enqueue */
	now = bpf_ktime_get_ns();
	delta = (now - *tsp);
	threshold = GETARG_FROM_ARRYMAP(argmap, argp, u64, threshold);
	if (threshold && delta <= threshold)
		return 0;

	event.cpuid = cpuid;
	event.pid = pid;
	event.prev_pid = prev_pid;
	event.delay = delta;
	event.stamp = now;
	bpf_probe_read(event.task, sizeof(event.task), &(ctx->next_comm));
	bpf_probe_read(event.prev_task, sizeof(event.prev_task), &(ctx->prev_comm));

	/* output */
	bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU,
			      &event, sizeof(event));

	bpf_map_delete_elem(&start, &pid);
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
