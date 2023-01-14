#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include "tracesig.bpf.h"
#include "../tracesig.h"

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 1);
	__type(key, u32);
	__type(value, struct filter);
} filtermap SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 20);
	__type(key, u32);
	__type(value, struct mypathbuf);
} path_map SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
	__uint(key_size, sizeof(u32));
	__uint(value_size, sizeof(u32));
} events SEC(".maps");

static inline int strequal(const char *src, const char *dst)
{
	bool ret = true;
	int i;
	unsigned char c1, c2;

	#pragma clang loop unroll(full)
	for (int i = 0; i < 16; i++) {
		c1 = *src++;
		c2 = *dst++;
		if (!c1 || !c2) {
			if (c1 != c2)
				ret = false;
			else
				ret = true;
			break;
		}
		if (c1 != c2) {
			ret = false;
			break;
		}
	}
	return ret;
}

SEC("kprobe/group_send_sig_info")
int BPF_KPROBE(group_send_sig_info, int sig, struct siginfo *info,
		struct task_struct *p, enum pid_type type)
{
	int k = 0, i = 0, j = 10;
	struct mypathbuf pathbuf, end;
	struct task_struct *cur, *pp;
	char comm[16], *name;
	struct path *path;
	struct filter *filterp;
	struct dentry *dentry, *parent;

	cur = (void *)(bpf_get_current_task());
	__builtin_memset(&end, 0, sizeof(end));
	pp = (struct task_struct *)BPF_CORE_READ(cur, parent);
	bpf_probe_read(end.parent, sizeof(end.parent), &(pp->comm));
	end.ppid = _(pp->pid);

	bpf_probe_read(end.dstcomm, sizeof(end.dstcomm), &(p->comm));
	end.dstpid = _(p->pid);

	dentry = (struct dentry *)BPF_CORE_READ(cur, fs, pwd.dentry);
	filterp = bpf_map_lookup_elem(&filtermap, &k);
	if (filterp && filterp->inited) {
		if (!strequal(end.dstcomm, filterp->comm))
			return 0;
	}
	#pragma clang loop unroll(full)
	for (i = 0; i < 10; i++) {
		__builtin_memset(&pathbuf, 0, sizeof(pathbuf));
		bpf_probe_read(pathbuf.d_iname, 39, &(dentry->d_iname));
		pathbuf.idx = i;
		j = j - pathbuf.idx;
		bpf_map_update_elem(&path_map, &j, &pathbuf, BPF_ANY);
		parent = BPF_CORE_READ(dentry, d_parent);
		if (dentry == parent)
			break;
		dentry = parent;
	}
	end.signum = sig;
	end.idx = END_MAGIC;
	end.pid = bpf_get_current_pid_tgid();
	bpf_get_current_comm(&end.comm, sizeof(end.comm));
	bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU,
	      &end, sizeof(end));

	return 0;
}

struct kill_argc {
	struct trace_entry ent;
	int __syscall_nr;
	pid_t pid;
	int sig;
	char __data[0];
};

char LICENSE[] SEC("license") = "GPL";
