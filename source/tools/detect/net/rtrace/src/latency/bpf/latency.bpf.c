#define BPF_NO_GLOBAL_DATA
#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_endian.h>

#include "common.h"
#include "bpf_core.h"

struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 32);
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

	r = (v > 0xFFFF) << 4; v >>= r;
	shift = (v > 0xFF) << 3; v >>= shift; r |= shift;
	shift = (v > 0xF) << 2; v >>= shift; r |= shift;
	shift = (v > 0x3) << 1; v >>= shift; r |= shift;
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

    if (pre_ts)
    {
        u64 delta = (bpf_ktime_get_ns() - *pre_ts) / 1000000;
        u32 idx = log2l(delta);

        u32 *count = bpf_map_lookup_elem(&hists, &idx);
        if (count)
            (*count)++;
    }
}

#define SKB_ENTRY_ARG_FN(pos)                                          \
    SEC("kprobe/skb_entry" #pos)                                       \
    int kprobe_skb_entry##pos(struct pt_regs *pt)                      \
    {                                                                  \
        struct sk_buff *skb = (struct sk_buff *)PT_REGS_PARM##pos(pt); \
        handle_skb_entry(pt, skb);                                     \
        return 0;                                                      \
    }

#define SKB_EXIT_ARG_FN(pos)                                           \
    SEC("kprobe/skb_exit" #pos)                                        \
    int kprobe_skb_exit##pos(struct pt_regs *pt)                       \
    {                                                                  \
        struct sk_buff *skb = (struct sk_buff *)PT_REGS_PARM##pos(pt); \
        handle_skb_exit(pt, skb);                                      \
        return 0;                                                      \
    }

SKB_ENTRY_ARG_FN(1)
SKB_ENTRY_ARG_FN(2)
SKB_ENTRY_ARG_FN(3)
SKB_ENTRY_ARG_FN(4)

SKB_EXIT_ARG_FN(1)
SKB_EXIT_ARG_FN(2)
SKB_EXIT_ARG_FN(3)
SKB_EXIT_ARG_FN(4)

char LICENSE[] SEC("license") = "Dual BSD/GPL";
