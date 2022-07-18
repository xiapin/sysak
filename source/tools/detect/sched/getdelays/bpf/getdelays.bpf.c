#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include "../getdelays.h"

/* We assume the number of threads of a process less than 2048 */
#define NR_MAPS	2048
#define TASK_RUNNING	0
#define _(P) ({typeof(P) val = 0; bpf_probe_read(&val, sizeof(val), &P); val;})
#define SYSCALL	0

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 4);
	__type(key, u32);
	__type(value, struct args);
} argmap SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, NR_MAPS);
	__type(key, u32);
	__type(value, struct irq_acct);
} irqmap SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, NR_MAPS);
	__type(key, struct syscall_key);
	__type(value, struct syscalls_acct);
} syscmap SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, NR_MAPS);
	__type(key, u32);
	__type(value, struct sys_ctx);
} context_map SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
	__uint(key_size, sizeof(u32));
	__uint(value_size, sizeof(u32));
} events SEC(".maps");

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


SEC("kretprobe/irq_enter")
int sysak_irq_enter_prog(struct pt_regs *ctx)
{
	u32 cpu, type;
	u32 pid, target_pid;
	struct irq_acct delays;
	struct task_struct *p;
	struct args *argp;

	p = (void *)bpf_get_current_task();
	target_pid = GETARG_FROM_ARRYMAP(argmap, argp, pid_t, targ_pid);
	type = GETARG_FROM_ARRYMAP(argmap, argp, int, type);
	if (type == TASKSTATS_CMD_ATTR_TGID)
		pid = _(p->tgid);
	else if (type == TASKSTATS_CMD_ATTR_PID)
		pid = _(p->pid);
	else
		return 0;
	if (pid == target_pid) {
		u64 now;
		struct irq_acct *delaysp;

		now = bpf_ktime_get_ns();
		delaysp = bpf_map_lookup_elem(&irqmap, &pid);
		if (!delaysp) {
			__builtin_memset(&delays, 0, sizeof(delays));
			delays.stamp = now;
			bpf_map_update_elem(&irqmap, &pid, &delays, 0);
		} else {
			delaysp->stamp = now;
		}
	}
	return 0;
}

SEC("kprobe/irq_exit")
int sysak_irq_exit_prog(struct pt_regs *ctx)
{
	u32 pid, type;
	struct args *argp;
	struct task_struct *p;
	struct irq_acct *delaysp;

	p = (void *)bpf_get_current_task();
	type = GETARG_FROM_ARRYMAP(argmap, argp, int, type);
	if (type == TASKSTATS_CMD_ATTR_TGID)
		pid = _(p->tgid);
	else if (type == TASKSTATS_CMD_ATTR_PID)
		pid = _(p->pid);
	else
		return 0;
	delaysp = bpf_map_lookup_elem(&irqmap, &pid);
	if (delaysp) {
		//int cpuid;
		u64 now, delta;

		now = bpf_ktime_get_ns();
		delta = now - delaysp->stamp;
		if (delta > delaysp->delay_max) {
			delaysp->delay_max = delta;
			delaysp->stamp = now;
		}
		//cpuid = bpf_get_smp_processor_id();
		/* todo:if delta > thesh then; touch event_poll */
		delaysp->delay = delaysp->delay + delta;
		delaysp->cnt = delaysp->cnt + 1;
		delaysp->pid = pid;
	}
	return 0;
}

SEC("tp/sched/sched_switch")
int handle__sched_switch(struct sched_sched_switch_args *args)
{
	u64 next_pid, prev_pid;
	u64 now, sleep, delay, wait, csw_begin;
	struct latency *acct;
	struct sys_ctx *ctx;

	__builtin_memset(&sleep, 0,sizeof(u64));
	__builtin_memset(&wait, 0,sizeof(u64));
	__builtin_memset(&csw_begin, 0,sizeof(u64));
	__builtin_memset(&now, 0,sizeof(u64));
	prev_pid = args->prev_pid;
	next_pid = args->next_pid;
	now = bpf_ktime_get_ns();
	ctx = bpf_map_lookup_elem(&context_map, &prev_pid);
	if (ctx) {
		u32 sysid;

		switch (_(ctx->stat)) {
		case SYSCALL:
			sysid = _(ctx->sysid);
			struct syscall_key key;
			struct syscalls_acct *delays;

			key.sysid = _(ctx->sysid);
			key.pid = prev_pid;
			delays = bpf_map_lookup_elem(&syscmap, &key);
			if (!delays)
				break;
			delays->csw_begin = now;
			delays->prev_state = args->prev_state;
			break;
		default:
			break;
		}
	}

	ctx = bpf_map_lookup_elem(&context_map, &next_pid);
	if (ctx) {
		u32 sysid;
		switch (_(ctx->stat)) {
		case SYSCALL:
			sysid = _(ctx->sysid);
			struct syscall_key key;
			struct syscalls_acct *delays;

			key.sysid = _(ctx->sysid);
			key.pid = next_pid;
			delays = bpf_map_lookup_elem(&syscmap, &key);
			if (!delays)
				break;
			sleep = _(delays->sleep);
			wait = _(delays->wait);
			csw_begin = _(delays->csw_begin);
			delay = now - csw_begin;
			if (delays->prev_state == TASK_RUNNING) {
				delays->wait = wait + delay;
			} else {
				delays->sleep = sleep + delay;
			}
			//if (delay > thresh)
			break;
		default:
			break;
		}
	}
ctsw_out:
	return 0;
}

SEC("tp/raw_syscalls/sys_enter")
int handle_raw_sys_enter(struct raw_sys_enter_arg *args)
{
	u32 cpu, type;
	u32 pid, target_pid;
	struct syscall_key key;
	struct syscalls_acct delays;
	struct task_struct *p;
	struct args *argp;

	p = (void *)bpf_get_current_task();
	target_pid = GETARG_FROM_ARRYMAP(argmap, argp, pid_t, targ_pid);
	type = GETARG_FROM_ARRYMAP(argmap, argp, int, type);
	if (type == TASKSTATS_CMD_ATTR_TGID)
		pid = _(p->tgid);
	else if (type == TASKSTATS_CMD_ATTR_PID)
		pid = _(p->pid);
	else
		return 0;
	if (pid == target_pid) {
		u64 now;
		struct sys_ctx ctx;
		struct syscalls_acct *delaysp;

		pid = _(p->pid);
		now = bpf_ktime_get_ns();
		key.pid = pid;
		key.sysid = args->id;
		delaysp = bpf_map_lookup_elem(&syscmap, &key);
		if (!delaysp) {
			__builtin_memset(&delays, 0, sizeof(delays));
			delays.tm_enter = now;
			bpf_map_update_elem(&syscmap, &key, &delays, 0);
		} else {
			delaysp->tm_enter = now;
		}
		ctx.stat = SYSCALL;
		ctx.sysid = key.sysid;
		bpf_map_update_elem(&context_map, &pid, &ctx, 0);
	}
	return 0;
}

SEC("tp/raw_syscalls/sys_exit")
int handle_raw_sys_exit(struct raw_sys_exit_arg *ctx)
{
	u32 target_pid, pid, tid, type;
	struct args *argp;
	struct task_struct *p;
	struct syscalls_acct *delaysp;
	struct syscall_key key;

	p = (void *)bpf_get_current_task();
	type = GETARG_FROM_ARRYMAP(argmap, argp, int, type);
	if (type == TASKSTATS_CMD_ATTR_TGID)
		pid = _(p->tgid);
	else if (type == TASKSTATS_CMD_ATTR_PID)
		pid = _(p->pid);
	else
		return 0;
	target_pid = GETARG_FROM_ARRYMAP(argmap, argp, pid_t, targ_pid);
	if (target_pid == pid) {
		pid = _(p->pid);
		key.pid = pid;
		key.sysid = ctx->id;
		delaysp = bpf_map_lookup_elem(&syscmap, &key);
		if (delaysp) {
			u64 now, delta;

			now = bpf_ktime_get_ns();
			delaysp->tm_exit = now;
			delta = now - delaysp->tm_enter;
			//cpuid = bpf_get_smp_processor_id();
			/* todo:if delta > thesh then; touch event_poll */
			delaysp->delay = delaysp->delay + delta;
			delaysp->cnt = delaysp->cnt + 1;
			if (delta > delaysp->delay_max) {
				delaysp->delay_max = delta;
				delaysp->max_enter = delaysp->tm_enter;
				delaysp->max_exit = delaysp->tm_exit;
			}
		}
		bpf_map_delete_elem(&context_map, &pid);
	}
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
