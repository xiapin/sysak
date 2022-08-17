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

enum {
    EVENT_UNKNOWN,
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
    u8 event_type;
};

struct filter
{
    u32 pid;
    u16 protocol;
    struct addr_pair ap;
};


#ifdef __VMLINUX_H__

static __always_inline void set_addr_pair_by_sock(struct sock *sk, struct addr_pair *ap)
{
    bpf_probe_read(&ap->daddr, sizeof(ap->daddr), &sk->__sk_common.skc_daddr);
    bpf_probe_read(&ap->dport, sizeof(ap->dport), &sk->__sk_common.skc_dport);
    bpf_probe_read(&ap->saddr, sizeof(ap->saddr), &sk->__sk_common.skc_rcv_saddr);
    bpf_probe_read(&ap->sport, sizeof(ap->sport), &sk->__sk_common.skc_num);
    ap->dport = bpf_ntohs(ap->dport);
}


#endif

#endif
