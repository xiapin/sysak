#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include "sched_jit.h"
#include "../pmubpf.h"

#define TASK_RUNNING	0
#define _(P) ({typeof(P) val = 0; bpf_probe_read(&val, sizeof(val), &P); val;})

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 4);
	__type(key, u32);
	__type(value, struct args);
} argmap SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 128);
	__type(key, u32);
	__type(value, u64);
} counter SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 128);
	__type(key, u32);
	__type(value, u64);
} pid_counter SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
	__uint(key_size, sizeof(u32));
	__uint(value_size, sizeof(u32));
} event SEC(".maps");

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

SEC("tp/sched/sched_switch")
int handle_switch(struct sched_switch_tp_args *ctx)
{
	struct args *argp;
	u32 pid, prev_pid, next_pid;
	u64 *prevp, *countp, this, delta;
	s64 error;
	int cpu;

	prev_pid = ctx->prev_pid;
	next_pid = ctx->next_pid;
	pid = GETARG_FROM_ARRYMAP(argmap, argp, u64, targ_pid);
	cpu = bpf_get_smp_processor_id();
	if (prev_pid == pid) {	/* switch out */
		prevp = bpf_map_lookup_elem(&counter, &cpu);
		if (prevp && (*prevp != 0)) {
			this = bpf_perf_event_read(&event, cpu);
			countp = bpf_map_lookup_elem(&pid_counter, &cpu);
			delta = this - *prevp;
			if (countp) {
				*countp = *countp + delta;
			} else {
				bpf_map_update_elem(&pid_counter, &cpu, &delta, 0);
			}
		} else {
			;/* do what ? */
		}
	}

	if (next_pid == pid) {	/* switch in */
		/* record the current counter of this cpu */
		prevp = bpf_map_lookup_elem(&counter, &cpu);
		if (prevp) {
			this = bpf_perf_event_read(&event, cpu);
			error = (s64)this;
			if (error <= -2 && error >= -22)
				return 0;
			*prevp = this;
		} else {
			this = bpf_perf_event_read(&event, cpu);
			bpf_map_update_elem(&counter, &cpu, &this, 0);
		}
	}

	return 0;
}

char LICENSE[] SEC("license") = "GPL";
