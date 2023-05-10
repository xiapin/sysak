#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include "../dynlinks.h"
#include "sched_jit.h"

#define MAX_ENTRIES	10240

#define DEQUEUE_SLEEP           0x01
#define DEQUEUE_SAVE            0x02
#define ENQUEUE_WAKEUP          0x01
#define ENQUEUE_RESTORE         0x02

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 1);
	__type(key, u32);
	__type(value, struct arg_info);
} arg_map SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_HASH);
	__uint(max_entries, MAX_ENTRIES);
	__type(key, u32);
	__type(value, struct value);
} test_map SEC(".maps");

struct {
	//__uint(type, BPF_MAP_TYPE_PERCPU_HASH);
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, MAX_ENTRIES);
	__type(key, u64);
	__type(value, struct value);
} info_map SEC(".maps");

#define _(P) ({typeof(P) val = 0; bpf_probe_read(&val, sizeof(val), &P); val;})

static inline u64 get_thresh(void)
{
	u64 thresh, i = 0;
	struct arg_info *argp;

	argp = bpf_map_lookup_elem(&arg_map, &i);
	if (argp)
		thresh = argp->thresh;
	else
		thresh = -1;

	return thresh;
}

SEC("kprobe/ttwu_do_wakeup")
int BPF_KPROBE(kp_ttwu_do_wakeup, struct rq *rq, struct task_struct *p, 
		int wake_flags, struct rq_flags *rf)
{
	u32 key;
	struct value value, *vp;

	key = _(p->tgid);
	vp = bpf_map_lookup_elem(&test_map, &key);
	if (vp) {
		vp->cnt++;
	} else {
		value.cnt = 1;
		bpf_map_update_elem(&test_map, &key, &value, BPF_ANY);
	}
	return 0;
}

SEC("kprobe/activate_task")
int BPF_KPROBE(activate_task, struct rq *rq, struct task_struct *p, int flags)
{
	u64 key;
	struct value value, *vp;
#if 1
	if (flags & ENQUEUE_RESTORE)
		return 0;
	if (!(flags & ENQUEUE_WAKEUP))
		return 0;
#endif

	key = _(p->tgid);
	vp = bpf_map_lookup_elem(&info_map, &key);
	if (vp) {
		vp->cnt++;
	} else {
		value.cnt = 1;
		bpf_map_update_elem(&info_map, &key, &value, BPF_ANY);
	}

	return 0;
}

SEC("kprobe/deactivate_task")
int BPF_KPROBE(deactivate_task, struct rq *rq, struct task_struct *p, int flags)
{
	u64 key;
	u64 cnt;
	struct value value, *vp;

#if 1
	if ((flags & DEQUEUE_SAVE))
		return 0;
	if (!(flags & DEQUEUE_SLEEP))
		return 0;
#endif

	key = _(p->tgid);
	vp = bpf_map_lookup_elem(&info_map, &key);
	if (vp) {
		cnt = vp->cnt;
		if (cnt > 0)
			vp->cnt = cnt+1;
		else
			vp->cnt = 0;
	}
	return 0;
}

struct sched_p_exit_args {
	struct trace_entry ent;
	char comm[16];
	pid_t pid;
	int prio;
};

#if 0
SEC("tp/sched/sched_process_exit")
int handle_switch(struct sched_p_exit_args *ctx)
{
	struct value *vp;
	u32 pid = _(ctx->pid);

	vp = bpf_map_lookup_elem(&info_map, &pid);
	if (vp)
		bpf_map_delete_elem(&info_map, &pid);
}
#endif

char LICENSE[] SEC("license") = "GPL";
