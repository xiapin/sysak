#define BPF_NO_GLOBAL_DATA
#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_endian.h>

#include "common.h"
#include "bpf_core.h"
#include "latency.h"

struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 10);
    __type(key, u32);
    __type(value, struct loghist);
} hists SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 102400);
    __type(key, struct sock *);
    __type(value, u64);
} inner_skbmap SEC(".maps");

static __always_inline u64 log2(u32 v)
{
    u32 shift, r;

    r = (v > 0xFFFF) << 4;
    v >>= r;
    shift = (v > 0xFF) << 3;
    v >>= shift;
    r |= shift;
    shift = (v > 0xF) << 2;
    v >>= shift;
    r |= shift;
    shift = (v > 0x3) << 1;
    v >>= shift;
    r |= shift;
    r |= (v >> 1);

    return r;
}

static __always_inline u64 log2l(u64 v)
{
    u32 hi = v >> 32;

    if (hi)
        return log2(hi) + 32;
    else
        return log2(v);
}

__always_inline void handle_skb_entry(void *ctx, struct sk_buff *skb)
{
    u64 ts = bpf_ktime_get_ns();
    bpf_map_update_elem(&inner_skbmap, &skb, &ts, BPF_ANY);
}

__always_inline void handle_skb_exit(void *ctx, struct sk_buff *skb)
{
    u64 *pre_ts = bpf_map_lookup_elem(&inner_skbmap, &skb);
    u32 key = 0;

    if (!pre_ts)
        return;

    struct loghist *lh = bpf_map_lookup_elem(&hists, &key);
    if (!lh)
        return;

    u64 delta = (bpf_ktime_get_ns() - *pre_ts) / 1000000;
    u32 idx = log2l(delta);
    asm volatile("%0 = %1"
                 : "=r"(idx)
                 : "r"(idx));
    if (idx < 32)
        __sync_fetch_and_add(&lh->hist[idx], 1);
}

SEC("kprobe/pfifo_fast_enqueue")
int BPF_KPROBE(pfifo_fast_enqueue, struct sk_buff *skb)
{
    handle_skb_entry(ctx, skb);
    return 0;
}

SEC("kretprobe/pfifo_fast_dequeue")
int BPF_KRETPROBE(pfifo_fast_dequeue, struct sk_buff *skb)
{
    handle_skb_exit(ctx, skb);
    return 0;
}

struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 16);
    __type(key, u64);
    __type(value, struct sockmap_value);
} sockmap SEC(".maps"); // or sockmap

struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 1);
    __type(key, u32);
    __type(value, struct pidmap_value);
} pidmap SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 16);
    __type(key, u32);
    __type(value, struct socktime);
} socktime_array SEC(".maps"); // or sockmap

struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, u32);
    __type(value, struct pidtime);
} pidtime_array SEC(".maps");

__always_inline u32 socktime_alloc_idx()
{
    struct socktime *st;
    u32 key = 0;
    u32 idx = 0;
    st = bpf_map_lookup_elem(&socktime_array, &key);
    if (st)
    {
        idx = st->sockevents[0].idx;
        st->sockevents[0].idx++;
    }
    return idx;
}

__always_inline u32 pidtime_alloc_idx()
{
    struct pidtime *pt;
    u32 key = 0;
    u32 idx = 0;
    pt = bpf_map_lookup_elem(&pidtime_array, &key);
    if (pt)
    {
        idx = pt->pidevents[0].idx;
        pt->pidevents[0].idx++;
    }
    return idx;
}

__always_inline void insert_sockmap(u64 key, u64 threshold)
{
    struct sockmap_value new_sock_val = {};
    new_sock_val.threshold = threshold;
    new_sock_val.socktime_idx = socktime_alloc_idx();
    bpf_map_update_elem(&sockmap, &key, &new_sock_val, BPF_ANY);
    bpf_printk("insert one flow, key: %u\n", key);
}

__always_inline void insert_pidmap_with_check(u32 pid)
{
    struct pidmap_value *pid_val;
    struct pidmap_value new_pid_val = {};
    
    // check if pid exist, or insert pid map.
    pid_val = bpf_map_lookup_elem(&pidmap, &pid);
    if (!pid_val)
    {
        new_pid_val.pidtime_idx = pidtime_alloc_idx();
        bpf_map_update_elem(&pidmap, &pid, &new_pid_val, BPF_ANY);
    }
}

__always_inline void insert_pidmap(u32 pid)
{
    struct pidmap_value new_pid_val = {};
    new_pid_val.pidtime_idx = pidtime_alloc_idx();
    bpf_map_update_elem(&pidmap, &pid, &new_pid_val, BPF_ANY);
}

__always_inline void insert_flow(u64 key, u64 threshold, u32 pid)
{
    bpf_printk("tracepoint\n");
    insert_sockmap(key, threshold);
    insert_pidmap_with_check(pid);
}

__always_inline struct filter *event_filter(struct addr_pair *ap, u32 pid)
{
    u32 key = 0;
    struct filter *filter;

    filter = bpf_map_lookup_elem(&filter_map, &key);
    if (!filter)
        return NULL;

    // pid not match
    if (filter->pid && (!pid || filter->pid != pid))
        return NULL;

    // address pair not match
    if (ap)
    {
        if (filter->ap.daddr && ap->daddr != filter->ap.daddr)
            return NULL;
        if (filter->ap.saddr && ap->saddr != filter->ap.saddr)
            return NULL;
        if (filter->ap.dport && ap->dport != filter->ap.dport)
            return NULL;
        if (filter->ap.sport && ap->sport != filter->ap.sport)
            return NULL;
    }

    return filter;
}

__always_inline u64 sockmap_key(struct sock *sk)
{
    u64 key = (u64)sk;
    if (bpf_core_field_exists(sk->__sk_common.skc_cookie))
        bpf_probe_read(&key, sizeof(key), &sk->__sk_common.skc_cookie);

    return key;
}

SEC("kprobe/tcp_rcv_established")
int BPF_KPROBE(tcp_rcv_established, struct sock *sk)
{
    struct sockmap_value *sock_val;
    struct addr_pair ap = {};
    struct filter *filter;
    u64 key = sockmap_key(sk);
    u32 pid;

    sock_val = bpf_map_lookup_elem(&sockmap, &key);
    if (!sock_val)
        return 0;

    sock_val->queue_ts = bpf_ktime_get_ns();
    return 0;
}

SEC("tracepoint/tcp/tcp_rcv_space_adjust")
int tp__tcp_rcv_space_adjust(struct trace_event_raw_tcp_event_sk *ctx)
{
    struct event event = {};
    struct sock *sk = ctx->skaddr;
    struct pidmap_value *pid_val;
    struct pidmap_value new_pid_val = {};
    struct sockmap_value *sock_val;
    struct sockmap_value new_sock_val = {};
    struct filter *filter;
    u64 ts, queue_ts, key = sockmap_key(sk);
    u32 pid;

    sock_val = bpf_map_lookup_elem(&sockmap, &key);
    if (!sock_val)
    {
        pid = bpf_get_current_pid_tgid();
        set_addr_pair_by_sock(sk, &event.ap);
        filter = event_filter(&event.ap, pid);
        if (!filter)
            return 0;

        insert_flow(key, filter->threshold, pid);
        return 0;
    }

    queue_ts = sock_val->queue_ts;
    if (queue_ts == 0)
        return 0;

    ts = bpf_ktime_get_ns();
    if (ts - queue_ts >= sock_val->threshold)
    {
        // clear queue timestmap to avoid outputing duplicated latency event
        sock_val->queue_ts = 0;

        // catch high latency
        event.type = LATENCY_EVENT;
        event.ts = ts;
        set_addr_pair_by_sock(sk, &event.ap);
        event.pid = bpf_get_current_pid_tgid();
        bpf_get_current_comm(&event.comm[0], sizeof(event.comm));
        event.socktime_array_idx = sock_val->socktime_idx;
        event.queue_ts = queue_ts;
        event.rcv_ts = ts;
        pid_val = bpf_map_lookup_elem(&pidmap, &event.pid);
        if (pid_val)
            event.pidtime_array_idx = pid_val->pidtime_idx;
        bpf_perf_event_output(ctx, &perf_map, BPF_F_CURRENT_CPU, &event, sizeof(event));
    }
    return 0;
}

#define ONE_SECOND_NS ((u64)(1000000000))

SEC("kprobe/sock_def_readable")
int BPF_KPROBE(sock_def_readable, struct sock *sk)
{
    u32 idx;
    u64 ts, delta, key = sockmap_key(sk);
    struct sockmap_value *sock_val;
    struct socktime *st;

    sock_val = bpf_map_lookup_elem(&sockmap, &key);
    if (!sock_val)
        return 0;

    idx = sock_val->socktime_idx;
    st = bpf_map_lookup_elem(&socktime_array, &idx);
    if (!st)
        return 0;

    ts = bpf_ktime_get_ns();

    seconds4_ring_insert(&st->sockevents[SOCK_EVENTS_SOCK_DEF_READABLE], ts);
    return 0;
}

SEC("tracepoint/sched/sched_switch")
int tp__sched_switch(struct trace_event_raw_sched_switch *ctx)
{
    struct pidmap_value *pid_val;
    struct pidtime *pt;
    u64 delta, ts;
    u32 idx, prev_pid = ctx->prev_pid, next_pid = ctx->next_pid;

    ts = bpf_ktime_get_ns();
    // sched out
    pid_val = bpf_map_lookup_elem(&pidmap, &prev_pid);
    if (pid_val)
    {
        idx = pid_val->pidtime_idx;
        pt = bpf_map_lookup_elem(&pidtime_array, &idx);
        if (pt)
            seconds4_ring_insert(&pt->pidevents[PID_EVENTS_SCHED_OUT], ts);
    }
    // sched in
    pid_val = bpf_map_lookup_elem(&pidmap, &next_pid);
    if (pid_val)
    {
        idx = pid_val->pidtime_idx;
        pt = bpf_map_lookup_elem(&pidtime_array, &idx);
        if (pt)
            seconds4_ring_insert(&pt->pidevents[PID_EVENTS_SCHED_IN], ts);
    }
    return 0;
}

SEC("kprobe/tcp_v4_destroy_sock")
int BPF_KPROBE(kprobe_tcp_v4_destroy_sock, struct sock *sk)
{
    struct sockmap_val *sock_val;
    u64 key = sockmap_key(sk);

    sock_val = bpf_map_lookup_elem(&sockmap, &key);
    if (sock_val)
        bpf_map_delete_elem(&sockmap, &key);

    return 0;
}


char LICENSE[] SEC("license") = "Dual BSD/GPL";
