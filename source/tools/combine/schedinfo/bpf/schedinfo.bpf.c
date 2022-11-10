#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include "../schedinfo.h"

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);	//it will have 128 elems
	__uint(max_entries, 128);	/* max_entries depends on the NR_CPUS */
	__type(key, u32);
	__type(value, struct rq_info);
} rq_map SEC(".maps");

#define _(P) ({typeof(P) val = 0; bpf_probe_read(&val, sizeof(val), &P); val;})

static inline int task_tick_pub(struct rq *rq, struct task_struct *p, int user_tick)
{
	struct rq_info *rqinfo, local;
	u32 cpuid;

	__builtin_memset(&local, 0, sizeof(struct rq_info));
	cpuid = bpf_get_smp_processor_id();
	rqinfo = bpf_map_lookup_elem(&rq_map, &cpuid);

	if (rqinfo) {
		rqinfo->nr_running = _(rq->nr_running);
		rqinfo->nr_uninterruptible = _(rq->nr_uninterruptible);
		rqinfo->rq_cpu_time = _(rq->rq_cpu_time);
		rqinfo->run_delay = BPF_CORE_READ(rq, rq_sched_info.run_delay);
		rqinfo->pcount = BPF_CORE_READ(rq, rq_sched_info.pcount);
		rqinfo->cpu = cpuid;
	} else {
		local.nr_running = _(rq->nr_running);
		local.nr_uninterruptible = _(rq->nr_uninterruptible);
		local.rq_cpu_time = _(rq->rq_cpu_time);
		local.cpu = cpuid;
		local.run_delay = BPF_CORE_READ(rq, rq_sched_info.run_delay);
		local.pcount = BPF_CORE_READ(rq, rq_sched_info.pcount);
		bpf_map_update_elem(&rq_map, &cpuid, &local, BPF_ANY);
	}

	return 0;
}

#define KPROBE_TASK_TICK(CLASS)				\
int BPF_KPROBE(task_tick_##CLASS, struct rq *rq,	\
	struct task_struct *p, int user_tick)		\
{							\
	return task_tick_pub(rq, p, user_tick);		\
}

SEC("kprobe/task_tick_stop")
KPROBE_TASK_TICK(stop)
SEC("kprobe/task_tick_dl")
KPROBE_TASK_TICK(dl)
SEC("kprobe/task_tick_rt")
KPROBE_TASK_TICK(rt)
SEC("kprobe/task_tick_fair")
KPROBE_TASK_TICK(fair)
SEC("kprobe/task_tick_idle")
KPROBE_TASK_TICK(idle)

char LICENSE[] SEC("license") = "Dual BSD/GPL";
