#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include "../appnoise.h"

#define NS_TO_US 1000
#define NS_TO_MS (NS_TO_US*1000)
#define NS_TO_S (NS_TO_MS*1000)

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 1);
	__type(key, u32);
	__type(value, struct args);
} filter_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, nr_hist);
	__type(key, u32);
	__type(value, struct histinfo);
} histmap SEC(".maps");
/*
    hist map;
    key-value
    0 - irq
    1 - softirq
    2 - nmi
    3 - wait
    4 - sleep
    5 - block
    6 - iowait
*/

struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
	__uint(max_entries, 4);
	__type(key, u32);
	__type(value, u64);
} start SEC(".maps");
/*
    0 - irq arrival time
    1 - softirq arrival time
*/

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, MAX_ENTRIES_IRQ);
	__type(key, u32);
	__type(value, struct irq_info_t);
} irq_infos SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, NR_SOFTIRQS);
	__type(key, u32);
	__type(value, struct info);
} softirq_infos SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, MAX_ENTRIES);
	__type(key, u32);
	__type(value, struct info);
    __uint(map_flags, BPF_F_NO_PREALLOC);
} syscall_infos SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, MAX_ENTRIES);
	__type(key, u32);
	__type(value, u64);
    __uint(map_flags, BPF_F_NO_PREALLOC);
} syscall_start SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, MAX_ENTRIES);
	__type(key, pid_t);
	__type(value,struct thread_info_t);
    __uint(map_flags, BPF_F_NO_PREALLOC);
}  numa_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, MAX_ENTRIES);
	__type(key, pid_t);
	__type(value, struct thread_info_t);
    __uint(map_flags, BPF_F_NO_PREALLOC);
} wait_thread_info SEC(".maps");

struct my_trace_event_raw_sys_enter {
	struct trace_entry ent;
	long int id;
	long unsigned int args[6];
	char __data[0];
};

struct my_trace_event_raw_sys_exit {
	struct trace_entry ent;
	long int id;
	long int ret;
	char __data[0];
};

struct my_trace_event_raw_irq_handler_entry {
	struct trace_entry ent;
	int irq;
	u32 __data_loc_name;
	char __data[0];
};

struct my_trace_event_raw_irq_handler_exit {
	struct trace_entry ent;
	int irq;
	int ret;
	char __data[0];
};

struct my_trace_event_raw_softirq {
	struct trace_entry ent;
	unsigned int vec;
	char __data[0];
};

struct my_trace_event_raw_nmi_handler {
	struct trace_entry ent;
	void *handler;
	s64 delta_ns;
	int handled;
	char __data[0];
};

static __always_inline u64 log2(u32 v)
{
	u32 shift, r;

	r = (v > 0xFFFF) << 4; v >>= r;
	shift = (v > 0xFF) << 3; v >>= shift; r |= shift;
	shift = (v > 0xF) << 2; v >>= shift; r |= shift;
	shift = (v > 0x3) << 1; v >>= shift; r |= shift;
	r |= (v >> 1);

	return r;
}

SEC("tp/irq/irq_handler_entry")
int handle_irq_entry(struct my_trace_event_raw_irq_handler_entry *ctx)
{
    u64 ts;
    u32 start_key = 0,key = 0,irq_key;
    pid_t pid,tgid;
    struct args *args;
    struct irq_info_t *info;
    struct irq_info_t zero = {};
    tgid = bpf_get_current_pid_tgid()>>32;
    pid = bpf_get_current_pid_tgid();
    args = bpf_map_lookup_elem(&filter_map,&key);

    if(!args)
        return 0;
    if(args->pid && args->pid != pid)
        return 0;
    if(args->tgid && args->tgid != tgid)
        return 0;

    ts = bpf_ktime_get_ns();
    bpf_map_update_elem(&start,&start_key,&ts,0);
    irq_key = ctx->irq;
    info = bpf_map_lookup_elem(&irq_infos,&irq_key);
    if(!info)
    {
        bpf_probe_read(&zero.name,sizeof(zero.name),ctx->__data);
        bpf_map_update_elem(&irq_infos,&irq_key,&zero,0);
        return 0;
    }
    return 0;
}

SEC("tp/irq/irq_handler_exit")
int handle_irq_exit(struct my_trace_event_raw_irq_handler_exit *ctx)
{
    u32 irq_key,key = 0,hist_key = hist_irq, start_key = 0;
    u64 duration,slot = 0;
    u64 *tsp;
    pid_t pid,tgid;
    struct args *args;
    struct irq_info_t *info;
    struct histinfo *histinfo;
    
    tgid = bpf_get_current_pid_tgid()>>32;
    pid = bpf_get_current_pid_tgid();
    args = bpf_map_lookup_elem(&filter_map,&key);
    if(!args)
        return 0;
    if(args->pid && args->pid != pid)
        return 0;
    if(args->tgid && args->tgid != tgid)
        return 0;

    tsp = bpf_map_lookup_elem(&start,&start_key);
    if(!tsp || !*tsp)
        return 0;

    duration = (bpf_ktime_get_ns() - *tsp)/NS_TO_US; 

    /* hist map data update */
    histinfo = bpf_map_lookup_elem(&histmap,&hist_key);
    if(!histinfo)
        return 0;
    __sync_fetch_and_add(&histinfo->count,1);
    __sync_fetch_and_add(&histinfo->total_time,duration);
    slot = log2(duration);
    if(slot >= MAX_SLOTS)
        slot = MAX_SLOTS-1;
    __sync_fetch_and_add(&histinfo->slots[slot],1);

    irq_key = ctx->irq;
    info = bpf_map_lookup_elem(&irq_infos,&irq_key);
    if(!info)
        return 0;
    __sync_fetch_and_add(&info->total_time,duration);
    __sync_fetch_and_add(&info->count,1);
    return 0;
}

SEC("tp/irq/softirq_entry")
int handler_softirq_entry(struct my_trace_event_raw_softirq *ctx)
{
    u32 key = 0,start_key = 1;
    u64 ts;
    pid_t pid,tgid;
    struct args *args;

    tgid = bpf_get_current_pid_tgid()>>32;
    pid = bpf_get_current_pid_tgid();
    args = bpf_map_lookup_elem(&filter_map,&key);
    if(!args)
        return 0;
    if(args->pid && args->pid != pid)
        return 0;
    if(args->tgid && args->tgid != tgid)
        return 0;

    ts = bpf_ktime_get_ns();
    bpf_map_update_elem(&start,&start_key,&ts,0);
    return 0;
}

SEC("tp/irq/softirq_exit")
int handler_softirq_exit(struct my_trace_event_raw_softirq *ctx)
{
    u32 key = 0,softirq_key,slot,hist_key = hist_softirq,start_key = 1;
    u64 duration;
    u64 *tsp;
    pid_t pid,tgid;
    struct args *args;
    struct histinfo* histinfo;
    struct info *info;

    tgid = bpf_get_current_pid_tgid()>>32;
    pid = bpf_get_current_pid_tgid();
    args = bpf_map_lookup_elem(&filter_map,&key);
    if(!args)
        return 0;
    if(args->pid && args->pid != pid)
        return 0;
    if(args->tgid && args->tgid != tgid)
        return 0;

    tsp = bpf_map_lookup_elem(&start,&start_key);
    if(!tsp || !*tsp)
        return 0;
    duration = (bpf_ktime_get_ns() - *tsp)/NS_TO_US; 

    histinfo = bpf_map_lookup_elem(&histmap,&hist_key);
    if(!histinfo)
        return 0;
    __sync_fetch_and_add(&histinfo->count,1);
    __sync_fetch_and_add(&histinfo->total_time,duration);
    slot = log2(duration);
    if(slot >= MAX_SLOTS)
        slot = MAX_SLOTS-1;
    __sync_fetch_and_add(&histinfo->slots[slot],1);

    softirq_key = ctx->vec;
    info = bpf_map_lookup_elem(&softirq_infos,&softirq_key);
    if(!info)
        return 0;
    __sync_fetch_and_add(&info->total_time,duration);
    __sync_fetch_and_add(&info->count,1);
    return 0;
}

SEC("tp/nmi/nmi_handler")
int handler_nmi(struct my_trace_event_raw_nmi_handler *ctx)
{
    u32 key = 0 , slot, hist_key = hist_nmi;
    s64 duration;
    pid_t pid,tgid;
    struct args *args;
    struct histinfo* histinfo;

    tgid = bpf_get_current_pid_tgid()>>32;
    pid = bpf_get_current_pid_tgid();
    args = bpf_map_lookup_elem(&filter_map,&key);
    if(!args)
        return 0;
    if(args->pid && args->pid != pid)
        return 0;
    if(args->tgid && args->tgid != tgid)
        return 0;
    
    duration = ((u64)ctx->delta_ns)/NS_TO_US;
    
    histinfo = bpf_map_lookup_elem(&histmap,&hist_key);
    if(!histinfo)
        return 0;

    __sync_fetch_and_add(&histinfo->count,1);
    __sync_fetch_and_add(&histinfo->total_time,duration);
    slot = log2(duration);
    if(slot >= MAX_SLOTS)
        slot = MAX_SLOTS-1;
    __sync_fetch_and_add(&histinfo->slots[slot],1);
    return 0;
}

SEC("raw_tp/sched_stat_wait")
int handler_sched_stat_wait(struct bpf_raw_tracepoint_args *ctx)
{
    struct args *args;
    struct histinfo* histinfo;
    pid_t pid,tgid,pid_prev,tgid_prev;
    struct thread_info_t *info,zero = {};
    u32 key = 0,slot, hist_key = hist_wait;
    u64 duration;
    struct task_struct *tsk = (void *)ctx->args[0];
    duration = ctx->args[1] / NS_TO_US;

    bpf_probe_read(&pid,sizeof(tsk->pid),&tsk->pid);
    bpf_probe_read(&tgid,sizeof(tsk->tgid),&tsk->tgid);

    args = bpf_map_lookup_elem(&filter_map,&key);
    if(!args)
        return 0;
    if(args->pid && args->pid != pid)
        return 0;
    if(args->tgid && args->tgid != tgid)
        return 0;

    histinfo = bpf_map_lookup_elem(&histmap,&hist_key);
    if(!histinfo)
        return 0;
    __sync_fetch_and_add(&histinfo->count,1);
    __sync_fetch_and_add(&histinfo->total_time,duration);
    slot = log2(duration);
    if(slot >= MAX_SLOTS)
        slot = MAX_SLOTS-1;
    __sync_fetch_and_add(&histinfo->slots[slot],1);

    tgid_prev = bpf_get_current_pid_tgid()>>32;
    pid_prev = bpf_get_current_pid_tgid();
    if(args->pid && args->pid == pid_prev)
        return 0;
    if(args->tgid && args->tgid == tgid_prev)
        return 0;

    info = bpf_map_lookup_elem(&wait_thread_info,&pid_prev);
    if(!info)
    {
        zero.pid = pid_prev;
        zero.count = 1;
        bpf_get_current_comm(zero.name,sizeof(zero.name));
        bpf_map_update_elem(&wait_thread_info,&pid_prev,&zero,0);
        return 0;
    }
    info->count++;
    return 0;
}

SEC("raw_tp/sched_stat_sleep")
int handler_sched_stat_sleep(struct bpf_raw_tracepoint_args *ctx)
{
    u32 key = 0, slot,hist_key = hist_sleep;
    u64 duration;
    pid_t pid,tgid;
    struct args *args;
    struct histinfo* histinfo;
    struct task_struct *tsk = (struct task_struct *)ctx->args[0];

    bpf_probe_read(&pid,sizeof(tsk->pid),&tsk->pid);
    bpf_probe_read(&tgid,sizeof(tsk->tgid),&tsk->tgid);

    args = bpf_map_lookup_elem(&filter_map,&key);
    if(!args)
        return 0;
    if(args->pid && args->pid != pid)
        return 0;
    if(args->tgid && args->tgid != tgid)
        return 0;

    duration = ctx->args[1] / NS_TO_S;

    histinfo = bpf_map_lookup_elem(&histmap,&hist_key);
    if(!histinfo)
        return 0;
    __sync_fetch_and_add(&histinfo->count,1);
    __sync_fetch_and_add(&histinfo->total_time,duration);
    slot = log2(duration);
    if(slot >= MAX_SLOTS)
        slot = MAX_SLOTS-1;
    __sync_fetch_and_add(&histinfo->slots[slot],1);


    return 0;
}

SEC("raw_tp/sched_stat_blocked")
int handler_sched_stat_blocked(struct bpf_raw_tracepoint_args *ctx)
{
    u32 key = 0, slot, hist_key = hist_block;
    u64 duration;
    pid_t pid,tgid;
    struct args *args;
    struct histinfo* histinfo;
    struct task_struct *tsk = (struct task_struct *)ctx->args[0];

    bpf_probe_read(&pid,sizeof(tsk->pid),&tsk->pid);
    bpf_probe_read(&tgid,sizeof(tsk->tgid),&tsk->tgid);

    args = bpf_map_lookup_elem(&filter_map,&key);
    if(!args)
        return 0;
    if(args->pid && args->pid != pid)
        return 0;
    if(args->tgid && args->tgid != tgid)
        return 0;

    duration = ctx->args[1] / NS_TO_S;

    histinfo = bpf_map_lookup_elem(&histmap,&hist_key);
    if(!histinfo)
        return 0;
    __sync_fetch_and_add(&histinfo->count,1);
    __sync_fetch_and_add(&histinfo->total_time,duration);
    slot = log2(duration);
    if(slot >= MAX_SLOTS)
        slot = MAX_SLOTS-1;
    __sync_fetch_and_add(&histinfo->slots[slot],1);

    return 0;
}

SEC("raw_tp/sched_stat_iowait")
int handler_sched_stat_iowait(struct bpf_raw_tracepoint_args *ctx)
{
    u32 key = 0, slot,hist_key = hist_iowait;
    u64 duration;
    pid_t pid,tgid;
    struct args *args;
    struct histinfo* histinfo;
    struct task_struct *tsk = (struct task_struct *)ctx->args[0];

    bpf_probe_read(&pid,sizeof(tsk->pid),&tsk->pid);
    bpf_probe_read(&tgid,sizeof(tsk->tgid),&tsk->tgid);

    args = bpf_map_lookup_elem(&filter_map,&key);
    if(!args)
        return 0;
    if(args->pid && args->pid != pid)
        return 0;
    if(args->tgid && args->tgid != tgid)
        return 0;

    duration = ctx->args[1] / NS_TO_S;

    histinfo = bpf_map_lookup_elem(&histmap,&hist_key);
    if(!histinfo)
        return 0;
    __sync_fetch_and_add(&histinfo->count,1);
    __sync_fetch_and_add(&histinfo->total_time,duration);
    slot = log2(duration);
    if(slot >= MAX_SLOTS)
        slot = MAX_SLOTS-1;
    __sync_fetch_and_add(&histinfo->slots[slot],1);
    return 0;
}

SEC("tp/raw_syscalls/sys_enter")
int handler_sys_enter(struct my_trace_event_raw_sys_enter *ctx)
{
    u64 ts;
    u32 key = 0,hist_key = hist_syscall;
    pid_t pid,tgid;
    struct args *args;
    tgid = bpf_get_current_pid_tgid()>>32;
    pid = bpf_get_current_pid_tgid();
    args = bpf_map_lookup_elem(&filter_map,&key);

    if(!args)
        return 0;
    if(args->pid && args->pid != pid)
        return 0;
    if(args->tgid && args->tgid != tgid)
        return 0;

    ts = bpf_ktime_get_ns();
    bpf_map_update_elem(&syscall_start,&pid,&ts,0);
    return 0;
}

SEC("tp/raw_syscalls/sys_exit")
int handler_sys_exit(struct my_trace_event_raw_sys_exit *ctx)
{
    u64 duration,tszero = 0,slot;
    u64 *tsp;
    u32 key = 0,hist_key = hist_syscall,id;
    pid_t pid,tgid;
    struct args *args;
    struct histinfo *histinfo;
    struct info *info;
    struct info zero = {};
    tgid = bpf_get_current_pid_tgid()>>32;
    pid = bpf_get_current_pid_tgid();
    args = bpf_map_lookup_elem(&filter_map,&key);

    if(!args)
        return 0;
    if(args->pid && args->pid != pid)
        return 0;
    if(args->tgid && args->tgid != tgid)
        return 0;

    tsp = bpf_map_lookup_elem(&syscall_start,&pid);
    if(!tsp || !*tsp)
        return 0;

    duration = (bpf_ktime_get_ns() - *tsp)/NS_TO_US;

    histinfo = bpf_map_lookup_elem(&histmap,&hist_key);
    if(!histinfo)
        return 0;
    __sync_fetch_and_add(&histinfo->count,1);
    __sync_fetch_and_add(&histinfo->total_time,duration);
    slot = log2(duration);
    if(slot >= MAX_SLOTS)
        slot = MAX_SLOTS-1;
    __sync_fetch_and_add(&histinfo->slots[slot],1);

    id = ctx->id;
    info = bpf_map_lookup_elem(&syscall_infos,&id);
    if(!info)
    {
        zero.count = 1;
        zero.total_time = duration;
        bpf_map_update_elem(&syscall_infos,&id,&zero,0);
        return 0;
    }
    __sync_fetch_and_add(&info->total_time,duration);
    __sync_fetch_and_add(&info->count,1);
    return 0;
}

SEC("kprobe/migrate_misplaced_page")
int migrate_misplaced_page_hook(struct pt_regs *ctx)
{
    struct args *args;
    int key = 0;
    pid_t pid,tgid;
    struct thread_info_t *info,zero = {};
    tgid = bpf_get_current_pid_tgid()>>32;
    pid = bpf_get_current_pid_tgid();
    args = bpf_map_lookup_elem(&filter_map,&key);
    if(!args)
        return 0;
    if(args->pid && args->pid != pid)
        return 0;
    if(args->tgid && args->tgid != tgid)
        return 0;
    info = bpf_map_lookup_elem(&numa_map,&pid);
    if(!info)
    {
        zero.count = 1;
        zero.pid = pid;
        bpf_get_current_comm(zero.name,sizeof(zero.name));
        bpf_map_update_elem(&numa_map,&pid,&zero,0);
        return 0;
    }
    __sync_fetch_and_add(&info->count,1);
    return 0;
}
char LICENSE[] SEC("license") = "GPL";
