#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include "../reclaimhung.h"

#define MAX_ENTRIES	10240


struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 10240);
	__type(key, u32);
	__type(value, struct reclaim_data);
} reclaim_map SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 10240);
	__type(key, u32);
	__type(value, struct compact_data);
} compact_map SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 10240);
	__type(key, u32);
	__type(value, struct reclaim_data);
} cgroup_map SEC(".maps");

struct trace_event_raw_mm_vmscan_cgroup_reclaim_begin {
    struct trace_entry ent;
    int order;
	int may_writepage;
    gfp_t gfp_flags;
	int classzone_idx;
    char __data[0];
};

struct trace_event_raw_mm_vmscan_cgroup_reclaim_end {
    struct trace_entry ent;
    long unsigned int nr_reclaimed;
    char __data[0];
};

SEC("tp/vmscan/mm_vmscan_direct_reclaim_begin")
int handle_direct_reclaim_begin(struct trace_event_raw_mm_vmscan_direct_reclaim_begin_template *rec)
{
	u64 current;
    u32 pid, tgid;	
	struct reclaim_data *entry, new;

    current = bpf_get_current_pid_tgid();
	__builtin_memset(&pid, 0, sizeof(u32));
    pid = current;
    tgid = current >> 32;
	entry = bpf_map_lookup_elem(&reclaim_map, &pid);
	if (!entry) {
		__builtin_memset(&new, 0, sizeof(struct reclaim_data));
		new.da.pid = pid;	
		new.da.tgid = tgid;
		new.da.time = bpf_ktime_get_ns();
		new.da.ts_begin = bpf_ktime_get_ns();
		new.da.ts_delay = 0;
		new.nr_reclaimed = 1;
		new.nr_pages = 0;
		new.cgroup = 0;
		bpf_get_current_comm(new.da.comm,TASK_COMM_LEN);
		bpf_map_update_elem(&reclaim_map, &pid, &new, BPF_ANY);
	} else {
		entry->da.time = bpf_ktime_get_ns();
		entry->da.ts_begin = bpf_ktime_get_ns();
		entry->nr_reclaimed++;

	}
	return 0;
}

SEC("tp/vmscan/mm_vmscan_direct_reclaim_end")
int handle_direct_reclaim_end(struct trace_event_raw_mm_vmscan_direct_reclaim_end_template *rec)
{
	u64 current;
    u32 pid;	
	struct reclaim_data *entry;

    current = bpf_get_current_pid_tgid();
	__builtin_memset(&pid, 0, sizeof(u32));
    pid = current;
	entry = bpf_map_lookup_elem(&reclaim_map, &pid);
	if (entry) {
		bpf_printk("in direct_compact_end\n");
		u64 now = bpf_ktime_get_ns();
		entry->da.ts_delay = now - entry->da.ts_begin;
		entry->nr_pages += rec->nr_reclaimed;
		bpf_get_current_comm(entry->da.comm,TASK_COMM_LEN);
	}
	return 0;
}

SEC("tp/compaction/mm_compaction_begin")
int handle_direct_compact_begin(struct trace_event_raw_mm_compaction_begin *com)
{
	u64 current;
    u32 pid, tgid;	
	struct compact_data *entry, new;

    current = bpf_get_current_pid_tgid();
	__builtin_memset(&pid, 0, sizeof(u32));
    pid = current;
	tgid = current >> 32;
	entry = bpf_map_lookup_elem(&compact_map, &pid);
	if (!entry) {
		__builtin_memset(&new, 0, sizeof(struct compact_data));
		new.da.pid = pid;	
		new.da.tgid = tgid;
		new.da.time = bpf_ktime_get_ns();
		new.da.ts_begin = bpf_ktime_get_ns();
		new.da.ts_delay = 0;
		new.nr_compacted = 1;
		bpf_get_current_comm(new.da.comm,TASK_COMM_LEN);
		bpf_map_update_elem(&compact_map, &pid, &new, BPF_NOEXIST);
	}
	else{
		entry->da.time = bpf_ktime_get_ns();
		entry->da.ts_begin = bpf_ktime_get_ns();
		entry->nr_compacted++;

	}
	return 0;
}

SEC("tp/compaction/mm_compaction_end")
int handle_direct_compact_end(struct trace_event_raw_mm_compaction_end *com)
{
	u64 current;
    u32 pid;	
	struct compact_data *entry;

    current = bpf_get_current_pid_tgid();
	__builtin_memset(&pid, 0, sizeof(u32));
    pid = current;
	entry = bpf_map_lookup_elem(&compact_map, &pid);
	if (entry) {
		u64 now = bpf_ktime_get_ns();
		entry->da.ts_delay = now - entry->da.ts_begin;
		entry->status = com->status;
		bpf_get_current_comm(entry->da.comm,TASK_COMM_LEN);
	}
	return 0;
}

SEC("tp/vmscan/mm_vmscan_memcg_reclaim_begin")
int handle_cgroup_reclaim_begin(struct trace_event_raw_mm_vmscan_cgroup_reclaim_begin *rec)
{
	u64 current;
    u32 pid, tgid;	
	struct reclaim_data *entry, new;

    current = bpf_get_current_pid_tgid();
	__builtin_memset(&pid, 0, sizeof(u32));
    pid = current;
    tgid = current >> 32;
	entry = bpf_map_lookup_elem(&cgroup_map, &pid);
	if (!entry) {
		__builtin_memset(&new, 0, sizeof(struct compact_data));
		new.da.pid = pid;	
		new.da.tgid = tgid;
		new.da.time = bpf_ktime_get_ns();
		new.da.ts_begin = bpf_ktime_get_ns();
		new.da.ts_delay = 0;
		new.nr_reclaimed = 1;
		new.nr_pages = 0;
		new.cgroup = 1;
		bpf_get_current_comm(new.da.comm,TASK_COMM_LEN);
		bpf_map_update_elem(&cgroup_map, &pid, &new, BPF_NOEXIST);
	}
	else{
		entry->da.time = bpf_ktime_get_ns();
		entry->da.ts_begin = bpf_ktime_get_ns();
		entry->nr_reclaimed++;

	}
	return 0;
}

SEC("tp/vmscan/mm_vmscan_memcg_reclaim_end")
int handle_cgroup_reclaim_end(struct trace_event_raw_mm_vmscan_cgroup_reclaim_end *rec)
{
	u64 current;
    u32 pid;	
	struct reclaim_data *entry;

    current = bpf_get_current_pid_tgid();
	__builtin_memset(&pid, 0, sizeof(u32));
    pid = current;
	entry = bpf_map_lookup_elem(&cgroup_map, &pid);
	if (entry) {
		u64 now = bpf_ktime_get_ns();
		entry->da.ts_delay = now - entry->da.ts_begin;
		entry->nr_pages += rec->nr_reclaimed;
		bpf_get_current_comm(entry->da.comm,TASK_COMM_LEN);
	}
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
