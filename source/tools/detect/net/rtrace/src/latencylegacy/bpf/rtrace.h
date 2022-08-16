#ifndef __RTRACE_H
#define __RTRACE_H

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

static __always_inline u64 fix_get_skb_tstamp(struct sk_buff *skb)
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
        struct msghdr___310 *msg310 = (void *)msg;;
        BPF_CORE_READ_INTO(&ptr, msg310, msg_iov, iov_base);
    }
    return ptr;
}

static __always_inline u16 bpf_core_sock_sk_protocol(struct sock *sk)
{
    return (u16)BPF_CORE_READ_BITFIELD_PROBED(sk, sk_protocol);
}

#endif

enum
{
    TCP_SEND_MSG = 0,
    TCP_TRANSMIT_SKB,
    TCP_IP_QUEUE_XMIT,
    TCP_NET_DEV_QUEUE,
    TCP_NET_DEV_XMIT,
    TCP_DEV_RCV,
    TCP_NETIF_RCV,
    TCP_QUEUE_RCV,
    TCP_CLEANUP_RBUF,
    TCP_ACK,
    __KFREE_SKB,
};

enum
{
    PING_SND = 0,
    PING_NET_DEV_QUEUE,
    PING_NET_DEV_XMIT,
    PING_DEV_RCV,
    PING_NETIF_RCV,
    PING_ICMP_RCV,
    PING_RCV,
    PING_KFREE_SKB,
};

#define MAX_TSTAMP_NUMBER 10

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

struct pid_info
{
    u32 pid;
    u8 comm[16];
};

struct addr_pair
{
    u32 saddr;
    u32 daddr;
    u16 sport;
    u16 dport;
};

struct address_info
{
    struct pid_info pi;
    struct addr_pair ap;
};

struct filter
{
    u16 protocol;
    u32 pid;
    struct addr_pair ap;
};

struct tcp_event
{
    u8 type;
    u64 sockaddr;
    u64 skbaddr;
    u64 ts;
    u64 skbts;

    u32 seq;
    u32 end_seq;
    u32 snd_una;
    u32 ack_seq;

    // u32 netns;
};

struct icmp_event
{
    u8 type;
    u8 icmp_type;
    u16 seq;
    u16 id;
    u64 ts;
    u64 skb_ts;
};

#endif
