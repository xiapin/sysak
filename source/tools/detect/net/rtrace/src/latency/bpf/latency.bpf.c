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
    __type(value, u32);
} hists SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 10240000);
    __type(key, struct sock *);
    __type(value, u64);
} skb_map SEC(".maps");

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
    bpf_map_update_elem(&skb_map, &skb, &ts, BPF_ANY);
}

__always_inline void handle_skb_exit(void *ctx, struct sk_buff *skb)
{
    u64 *pre_ts = bpf_map_lookup_elem(&skb_map, &skb);
    u32 key = 0;

    if (pre_ts)
    {
        u64 delta = (bpf_ktime_get_ns() - *pre_ts) / 1000000;
        u32 idx = log2l(delta);

        struct loghist *lh = bpf_map_lookup_elem(&hists, &key);
        if (lh)
            if (idx < 32)
                lh->hist[idx]++;
    }
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


char LICENSE[] SEC("license") = "Dual BSD/GPL";
