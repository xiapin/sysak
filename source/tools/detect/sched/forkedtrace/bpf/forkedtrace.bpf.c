#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include "../forkedtrace.h"

#define _(P) ({typeof(P) val = 0; bpf_probe_read(&val, sizeof(val), &P); val;})

struct my_sched_wakeup_ctx {
	struct trace_entry ent;
	char comm[16];
	pid_t pid;;
	int prio;
	int success;
	int target_cpu;
	char __data[0];
};

struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
	__uint(max_entries, 1);
	__type(key, u32);
	__type(value, struct wake_account);
} cnt_map SEC(".maps");

struct bpf_map_def SEC("maps") info_map = {
	.type = BPF_MAP_TYPE_HASH,
	.key_size = sizeof(int),
	.value_size = sizeof(struct wake_up_data),
	.max_entries = 4096,
};

SEC("tp/sched/sched_wakeup_new")
int sysak_handle_sched_wakeup_new(struct my_sched_wakeup_ctx *ctx)
{
	u32 tgid;	/* if we use tgid, we have to think aout percpu */
	u32 ppid, pid, key = 0;
	struct task_struct *task;
	struct wake_up_data ttwu, *ttwup;
	struct wake_account *wactp;

	task = (void *)bpf_get_current_task();
	pid = _(task->pid);
	ppid = BPF_CORE_READ(task, parent, pid);
	/*tgid = task->tgid;
	 *tgid = bpf_get_current_pid_tgid() >> 32;
	 *pid = bpf_get_current_pid_tgid();
	 */
	ttwup = bpf_map_lookup_elem(&info_map, &pid);
	if (ttwup) {
		ttwup->new_cnt++;
	} else {
		__builtin_memset(&ttwu, 0, sizeof(struct wake_up_data));
		ttwu.new_cnt++;
		ttwu.wakee = pid;
		ttwu.ppid = ppid;
		bpf_get_current_comm(&ttwu.comm, sizeof(ttwu.comm));
		bpf_map_update_elem(&info_map, &pid, &ttwu, BPF_ANY);	//for percpu, should we use BPF_NOEXIST?
	}

	wactp = bpf_map_lookup_elem(&cnt_map, &key);
	if (wactp) {
		wactp->new_cnt++;
	}
	return 0;
}

#if 0
SEC("tp/sched/sched_wakeup")
int sysak_handle_sched_wakeup(struct my_sched_wakeup_ctx *ctx)
{
	u32 key = 0;
	struct wake_account *wactp;
	
	__builtin_memset(&key, 0, sizeof(u32));
	wactp = bpf_map_lookup_elem(&cnt_map, &key);
	if (wactp) {
		wactp->wake_cnt++;
	}
	return 0;
}
#endif

char LICENSE[] SEC("license") = "Dual BSD/GPL";

