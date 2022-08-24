#ifndef __COMMON_H
#define __COMMON_H

#ifndef u8
typedef unsigned char u8;
#endif

#ifndef u16
typedef unsigned short int u16;
#endif

#ifndef u32
typedef unsigned int u32;
#endif

#ifndef u64
typedef long long unsigned int u64;
#endif

enum
{
    DROP_KFREE_SKB = 0,
    DROP_TCP_DROP,
    DROP_IPTABLES_DROP,
    DROP_NFCONNTRACK_DROP,
    LATENCY_EVENT,
    EVENT_UNKNOWN,
};

enum
{
    ERR_OK = 0,
    ERR_PROTOCOL_NOT_DETERMINED,
    ERR_PROTOCOL_NOT_SUPPORT,
};

struct addr_pair
{
    u32 saddr;
    u32 daddr;
    u16 sport;
    u16 dport;
};

struct event
{
    // Event Type
    u8 type;
    // Sock state
    u8 state;
    u8 protocol;
    u8 error;
    // process command
    u8 comm[16];
    // stack id
    u32 stackid;
    u32 pid;
    u64 ts;
    struct addr_pair ap;

    // Don't move anonymous structs before and after
    union
    {
        // 1. for latency module
        struct
        {
            u32 pidtime_array_idx;
            u32 socktime_array_idx;
            u64 queue_ts;
            u64 rcv_ts;
        };

        // 2. for drop module
        struct
        {
            // iptables table name
            u8 name[32];
            // iptables hook chain name
            u32 hook;
            u8 sk_protocol;
            struct addr_pair skap;
        } drop_params;

        // 3. for abnormal module
        struct
        {
            u32 i_ino;
            // queue
            // length of accept queue
            u32 sk_ack_backlog;
            // length of syn queue
            u32 icsk_accept_queue;
            u32 sk_max_ack_backlog;

            // memory
            u32 sk_wmem_queued;
            u32 sndbuf;
            u32 rmem_alloc;
            u32 sk_rcvbuf;

            u32 drop;
            u32 retran;
            u32 ooo;
        } abnormal;
    };
    // rcvnxt
};

struct filter
{
    u32 pid;
    u16 protocol;
    /* get latency distribution or not? */
    u8 distribution;
    u64 threshold;
    struct addr_pair ap;
};

struct onesecond
{
    u64 ts; // initial timestamp
    u32 clear;
    u32 bitmap[32]; // 1 second bitmap
};

struct seconds4_ring
{
    struct onesecond os[4];
    u32 idx;
};

#ifdef __VMLINUX_H__

struct
{
    __uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
    __uint(key_size, sizeof(u32));
    __uint(value_size, sizeof(u32));
} perf_map SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, u32);
    __type(value, struct filter);
} filter_map SEC(".maps");

static __always_inline void set_addr_pair_by_sock(struct sock *sk, struct addr_pair *ap)
{
    bpf_probe_read(&ap->daddr, sizeof(ap->daddr), &sk->__sk_common.skc_daddr);
    bpf_probe_read(&ap->dport, sizeof(ap->dport), &sk->__sk_common.skc_dport);
    bpf_probe_read(&ap->saddr, sizeof(ap->saddr), &sk->__sk_common.skc_rcv_saddr);
    bpf_probe_read(&ap->sport, sizeof(ap->sport), &sk->__sk_common.skc_num);
    ap->dport = bpf_ntohs(ap->dport);
}

static __always_inline void onesecond_insert(volatile struct onesecond *os, u64 ns)
{
    u32 msec = ns / (1000 * 1000);
    volatile u32 idx = msec / 32;

    if (os->clear & (1 << idx))
    {
        os->bitmap[idx & 0x1f] |= (1 << (msec & 0x1f));
    }
    else
    {
        os->clear |= (1 << idx);
        os->bitmap[idx & 0x1f] = (1 << (msec & 0x1f));
    }
}

static __always_inline void seconds4_ring_insert(struct seconds4_ring *sr, u64 ts)
{
    u32 idx = sr->idx;
    u32 prets = sr->os[idx & 0x3].ts;
    u64 delta = ts - prets;
    if (delta >= 1000 * 1000 * 1000)
    {
        idx++;
        sr->idx = idx;
        sr->os[idx & 0x3].ts = ts;
        sr->os[idx & 0x3].clear = 0;
        sr->os[idx & 0x3].bitmap[0] = 0;
    }

    onesecond_insert(&sr->os[idx & 0x3], delta);
}

#endif

#endif
