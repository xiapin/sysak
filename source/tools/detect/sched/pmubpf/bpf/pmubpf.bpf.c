#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>
#include "sched_jit.h"
#include "../pmubpf.h"
#define MAX_CON		512
#define MAX_CPUS	256

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
} task_schedin SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 128);
	__type(key, u32);
	__type(value, u64);
} task_counter SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_HASH);
	__uint(max_entries, MAX_CON*MAX_CPUS);
	__type(key, struct cg_key);
	__type(value, u64);
} cg_schedin SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 512);
	__type(key, struct cg_key);
	__type(value, u64);
} cg_counter SEC(".maps");

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

#define PERF_SUBSYS_ID	1

#if 0
union kernfs_node_id {
	struct {
		u32 ino;
		u32 generation;
	};
	u64 id;
};

struct kernfs_node___419 {
	atomic_t count;
	atomic_t active;
	struct kernfs_node *parent;
	const char *name;
	struct rb_node rb;
	const void *ns;
	unsigned int hash;
	union {
		struct kernfs_elem_dir dir;
		struct kernfs_elem_symlink symlink;
		struct kernfs_elem_attr attr;
	};
	void *priv;
	union kernfs_node_id id;
	short unsigned int flags;
	umode_t mode;
	struct kernfs_iattrs *iattr;
};

static u64 get_cgroup_id(struct task_struct *t)
{
	struct cgroup *cgrp;
	struct kernfs_node___419 *node;
	union kernfs_node_id id;
	u64 knid;

	cgrp = BPF_CORE_READ(t, cgroups, subsys[PERF_SUBSYS_ID], cgroup);
	if (bpf_core_read(&node, sizeof(struct kernfs_node___419 *), &cgrp->kn))
		return 0;
	if (bpf_core_read(&id, sizeof(union kernfs_node_id), &node->id))
		return 0;
	if (bpf_core_read(&knid, sizeof(u64), &id.id))
		return 0;

	return knid;
}
#endif

#if 1
static inline __u64 get_cgroup_id(struct task_struct *t)
{
	struct cgroup *cgrp;

	cgrp = BPF_CORE_READ(t, cgroups, subsys[PERF_SUBSYS_ID], cgroup);
	return BPF_CORE_READ(cgrp, kn, id);
}
#endif

SEC("raw_tracepoint/sched_switch")
int sysak_pmubpf__sched_switch(struct bpf_raw_tracepoint_args *ctx)
{
	struct cg_key pkey, nkey;
	bool preempt = (bool)(ctx->args[0]);
	struct task_struct *prev, *next;
	u32 cpu = bpf_get_smp_processor_id();

	__builtin_memset(&pkey, 0, sizeof(struct cg_key));
	__builtin_memset(&nkey, 0, sizeof(struct cg_key));
	prev = (struct task_struct *)(ctx->args[1]);
	next = (struct task_struct *)(ctx->args[2]);
	
	pkey.cgid = get_cgroup_id(prev);
	nkey.cgid = get_cgroup_id(next);

	if (nkey.cgid != pkey.cgid) {	/* cgroup changed  */
		s64 delta = 0;
		u64 *valp, *last, val;

		last = bpf_map_lookup_elem(&cg_schedin, &pkey);
		if (last && (*last != 0)) {
			val = bpf_perf_event_read(&event, cpu);
			delta = val - *last;
			valp = bpf_map_lookup_elem(&cg_counter, &pkey);
			if (valp)
				*valp = *valp + delta;
			else
				bpf_map_update_elem(&cg_counter, &pkey, &delta, 0);
		}

		/* record the start value of new sched_in task */
		val = bpf_perf_event_read(&event, cpu);
		bpf_map_update_elem(&cg_schedin, &nkey, &val, 0);
	}
	return 0;
}

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
		prevp = bpf_map_lookup_elem(&task_schedin, &cpu);
		if (prevp && (*prevp != 0)) {
			this = bpf_perf_event_read(&event, cpu);
			countp = bpf_map_lookup_elem(&task_counter, &cpu);
			delta = this - *prevp;
			if (countp) {
				*countp = *countp + delta;
			} else {
				bpf_map_update_elem(&task_counter, &cpu, &delta, 0);
			}
		} else {
			;/* do what ? */
		}
	}

	if (next_pid == pid) {	/* switch in */
		/* record the current counter of this cpu */
		prevp = bpf_map_lookup_elem(&task_schedin, &cpu);
		if (prevp) {
			this = bpf_perf_event_read(&event, cpu);
			error = (s64)this;
			if (error <= -2 && error >= -22)
				return 0;
			*prevp = this;
		} else {
			this = bpf_perf_event_read(&event, cpu);
			bpf_map_update_elem(&task_schedin, &cpu, &this, 0);
		}
	}

	return 0;
}

#if 0
/* switch in */
SEC("kprobe/finish_task_switch")

#endif

char LICENSE[] SEC("license") = "GPL";
