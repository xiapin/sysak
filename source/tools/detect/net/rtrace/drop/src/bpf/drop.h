#ifndef __DROP_H
#define __DROP_H
#include "common.h"

#define IPTABLE_FILTER 1
#define IPTABLE_MANGLE 2
#define IPTABLE_RAW 3
#define ARPTABLE_FILTER 4  
#define IPTABLE_SECURITY 5
#define IPTABLE_NAT 6


enum {
    NF_CONNTRACK,
    IPTABLES,
    KFREE_SKB,
    TCP_DROP,
    TP_KFREE_SKB,
};

struct drop_filter
{
    u16 protocol;
    struct addr_pair ap;
};

#if 0
struct drop_event
{
    // Event Type
    u8 type;
    u8 has_sk;
    // Sock state
    u8 sk_state;
    u8 sk_protocol;
    u16 cpu;
    struct addr_pair skap;

    // skb state
    u8 skb_protocol;
    struct addr_pair skbap;

    u32 pid;
    // stack id
    u32 stackid;
    // process command
    u8 comm[16];
    u64 ts;
    // iptables table name
    u8 name[32];
    // iptables hook chain name
    u32 hook;

    u64 location;
};

#endif

struct drop_event
{
    u64 location;
    u16 proto;
    struct addr_pair ap;
};

#endif
