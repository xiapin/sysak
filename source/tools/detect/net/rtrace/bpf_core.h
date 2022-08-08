#ifndef __BPF_CORE_H
#define __BPF_CORE_H

#ifdef __VMLINUX_H__

union ktime___310
{
    s64 tv64;
};
typedef union ktime___310 ktime_t___310;
struct sk_buff___310
{
    ktime_t___310 tstamp;
};

static __always_inline u64 bpf_core_skb_tstamp(struct sk_buff *skb)
{
    u64 ts;
    ktime_t___310 ktime310;
    if (bpf_core_field_exists(ktime310.tv64))
    {
        struct sk_buff___310 *skb310 = (void *)skb;
        bpf_core_read(&ktime310, sizeof(ktime310), &skb310->tstamp);
        ts = ktime310.tv64;
    }
    else
    {
        bpf_probe_read(&ts, sizeof(u64), &skb->tstamp);
    }
    return ts;
}

struct msghdr___310
{
    struct iovec *msg_iov;
};

// libbpf: prog 'kprobe__raw_sendmsg': relo #3: kind <byte_off> (0), spec is [346] struct msghdr.msg_iter.iov (0:2:4:0 @ offset 40)
static __always_inline void *fix_msghdr_base(struct msghdr *msg)
{
    void *ptr;
    if (bpf_core_field_exists(msg->msg_iter))
    {
        BPF_CORE_READ_INTO(&ptr, msg, msg_iter.iov, iov_base);
    }
    else
    {
        struct msghdr___310 *msg310 = (void *)msg;
        ;
        BPF_CORE_READ_INTO(&ptr, msg310, msg_iov, iov_base);
    }
    return ptr;
}

static __always_inline u16 bpf_core_sock_sk_protocol(struct sock *sk)
{
    return (u16)BPF_CORE_READ_BITFIELD_PROBED(sk, sk_protocol);
}

struct netns_ipv4___310_419
{
    struct xt_table *iptable_filter;
    struct xt_table *iptable_mangle;
    struct xt_table *iptable_raw;
    struct xt_table *arptable_filter;
    struct xt_table *iptable_security;
    struct xt_table *nat_table;
};

static __always_inline u64 bpf_core_netns_ipv4_iptable_filter(void *ptr)
{
    struct netns_ipv4___310_419 *ns = ptr;
    u64 addr = 0;
    if (bpf_core_field_exists(ns->iptable_filter))
        bpf_probe_read(&addr, sizeof(addr), &ns->iptable_filter);

    return addr;
}

static __always_inline u64 bpf_core_netns_ipv4_iptable_mangle(void *ptr)
{
    struct netns_ipv4___310_419 *ns = ptr;
    u64 addr = 0;
    if (bpf_core_field_exists(ns->iptable_mangle))
        bpf_probe_read(&addr, sizeof(addr), &ns->iptable_mangle);

    return addr;
}

static __always_inline u64 bpf_core_netns_ipv4_iptable_raw(void *ptr)
{
    struct netns_ipv4___310_419 *ns = ptr;
    u64 addr = 0;
    if (bpf_core_field_exists(ns->iptable_raw))
        bpf_probe_read(&addr, sizeof(addr), &ns->iptable_raw);

    return addr;
}

static __always_inline u64 bpf_core_netns_ipv4_arptable_filter(void *ptr)
{
    struct netns_ipv4___310_419 *ns = ptr;
    u64 addr = 0;
    if (bpf_core_field_exists(ns->arptable_filter))
        bpf_probe_read(&addr, sizeof(addr), &ns->arptable_filter);

    return addr;
}

static __always_inline u64 bpf_core_netns_ipv4_iptable_security(void *ptr)
{
    struct netns_ipv4___310_419 *ns = ptr;
    u64 addr = 0;
    if (bpf_core_field_exists(ns->iptable_security))
        bpf_probe_read(&addr, sizeof(addr), &ns->iptable_security);

    return addr;
}

static __always_inline u64 bpf_core_netns_ipv4_nat_table(void *ptr)
{
    struct netns_ipv4___310_419 *ns = ptr;
    u64 addr = 0;
    if (bpf_core_field_exists(ns->nat_table))
        bpf_probe_read(&addr, sizeof(addr), &ns->nat_table);

    return addr;
}

#define XT_TABLE_MAXNAMELEN 32
struct xt_table___419
{
    char name[XT_TABLE_MAXNAMELEN];
};

static __always_inline u64 bpf_core_xt_table_name(void *ptr)
{
    struct xt_table___419 *table = ptr;
    if (bpf_core_field_exists(table->name))
        return (u64)(&table->name[0]);
    return 0;
}

struct listen_sock___310
{
    int qlen;
};

struct request_sock_queue___310
{
    struct listen_sock___310 *listen_opt;
};

static __always_inline u32 bpf_core_reqsk_synqueue_len(struct sock *sk)
{
    u32 synqueue_len = 0;
    struct request_sock_queue___310 *reqsk310 = &((struct inet_connection_sock *)sk)->icsk_accept_queue;
    if (bpf_core_field_exists(reqsk310->listen_opt))
        BPF_CORE_READ_INTO(&synqueue_len, reqsk310, listen_opt, qlen);
    else
    {
        struct request_sock_queue *reqsk = reqsk310;
        bpf_probe_read(&synqueue_len, sizeof(synqueue_len), &reqsk->qlen.counter);
    }

    return synqueue_len;
}
#endif

#endif
