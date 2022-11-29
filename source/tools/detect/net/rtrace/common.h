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

#define MAX_STACK_DEPTH     20        // max depth of each stack trace to track

enum
{
    DROP_KFREE_SKB = 0,
    DROP_TCP_DROP,
    DROP_IPTABLES_DROP,
    DROP_NFCONNTRACK_DROP,
    LATENCY_EVENT,
    CONNECT_LATENCY_EVENT,
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

        // 4. for connect latency
        struct 
        {
            u64 sock;
        } connectlatency;
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

// return u64
#define ns() bpf_ktime_get_ns()
// return u32
#define pid() (bpf_get_current_pid_tgid() >> 32)
// return u32
#define tid() ((u32)bpf_get_current_pid_tgid())
#define COMM(comm) bpf_get_current_comm(comm, sizeof(comm))
#define comm(c) COMM(c)
// return u32
#define cpu() bpf_get_smp_processor_id()

// https://github.com/aquasecurity/tracee/blob/main/pkg/ebpf/c/tracee.bpf.c
#define BPF_MAP(_name, _type, _key_type, _value_type, _max_entries) \
    struct                                                          \
    {                                                               \
        __uint(type, _type);                                        \
        __uint(max_entries, _max_entries);                          \
        __type(key, _key_type);                                     \
        __type(value, _value_type);                                 \
    } _name SEC(".maps");

#define BPF_HASH(_name, _key_type, _value_type, _max_entries) \
    BPF_MAP(_name, BPF_MAP_TYPE_HASH, _key_type, _value_type, _max_entries)

#define BPF_LRU_HASH(_name, _key_type, _value_type, _max_entries) \
    BPF_MAP(_name, BPF_MAP_TYPE_LRU_HASH, _key_type, _value_type, _max_entries)

#define BPF_ARRAY(_name, _value_type, _max_entries) \
    BPF_MAP(_name, BPF_MAP_TYPE_ARRAY, u32, _value_type, _max_entries)

#define BPF_PERCPU_ARRAY(_name, _value_type, _max_entries) \
    BPF_MAP(_name, BPF_MAP_TYPE_PERCPU_ARRAY, u32, _value_type, _max_entries)

#define BPF_PROG_ARRAY(_name, _max_entries) \
    BPF_MAP(_name, BPF_MAP_TYPE_PROG_ARRAY, u32, u32, _max_entries)

#define BPF_PERF_OUTPUT(_name, _max_entries) \
    BPF_MAP(_name, BPF_MAP_TYPE_PERF_EVENT_ARRAY, int, __u32, _max_entries)

// stack traces: the value is 1 big byte array of the stack addresses
typedef __u64 stack_trace_t[MAX_STACK_DEPTH];
#define BPF_STACK_TRACE(_name, _max_entries)                                                       \
    BPF_MAP(_name, BPF_MAP_TYPE_STACK_TRACE, u32, stack_trace_t, _max_entries)


#define READ_KERN(ptr)                                    \
    ({                                                    \
        typeof(ptr) _val;                                 \
        __builtin_memset((void *)&_val, 0, sizeof(_val)); \
        bpf_core_read((void *)&_val, sizeof(_val), &ptr); \
        _val;                                             \
    })

#define READ_USER(ptr)                                         \
    ({                                                         \
        typeof(ptr) _val;                                      \
        __builtin_memset((void *)&_val, 0, sizeof(_val));      \
        bpf_core_read_user((void *)&_val, sizeof(_val), &ptr); \
        _val;                                                  \
    })

struct
{
    __uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
    __uint(key_size, sizeof(u32));
    __uint(value_size, sizeof(u32));
} perf_map SEC(".maps");

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
