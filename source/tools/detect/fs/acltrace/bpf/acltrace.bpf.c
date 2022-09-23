#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include "../acltrace.h"

#define MAX_ENTRIES	1024
#define DNAME_INLINE_LEN 40

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, MAX_ENTRIES);
	__type(key, u32);
	__type(value, struct acl_data);
} acl_map SEC(".maps");

SEC("kprobe/ext4_xattr_set_acl")
int bpf_prog(struct pt_regs *ctx)
{
	u64 current;
    u32 pid, tgid;
	struct acl_data *entry, new;
	struct dentry *dentry;
	const char *name;

	dentry = (struct dentry *) PT_REGS_PARM1(ctx);
	name = (char *) PT_REGS_PARM2(ctx);

    current = bpf_get_current_pid_tgid();
	__builtin_memset(&pid, 0, sizeof(u32));
    pid = current;
    tgid = current >> 32;
	entry = bpf_map_lookup_elem(&acl_map, &pid);
	if (!entry) {
		__builtin_memset(&new, 0, sizeof(struct acl_data));
		new.pid = pid;	
		new.tgid = tgid;
		new.count = 1;
		new.time = bpf_ktime_get_ns();
		bpf_get_current_comm(new.comm,TASK_COMM_LEN);
		bpf_probe_read_kernel_str(&new.dentryname, sizeof(new.dentryname), dentry->d_iname);
		bpf_probe_read_kernel_str(&new.xattrs, sizeof(new.xattrs), name);
		bpf_map_update_elem(&acl_map, &pid, &new, BPF_ANY);
	} else {
		entry->time = bpf_ktime_get_ns();
		entry->count++;
		bpf_probe_read_kernel_str(&new.dentryname, sizeof(new.dentryname), dentry->d_iname);
		bpf_probe_read_kernel_str(&new.xattrs, sizeof(new.dentryname), name);
	}
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
