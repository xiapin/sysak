#define BPF_NO_GLOBAL_DATA
#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_endian.h>

#include "common.h"
#include "bpf_core.h"
#include "icmp.h"


__always_inline void get_icmphdr_with_l4(struct sk_buff *skb, struct icmphdr *ih)
{
    char *head;
    u16 transport_header;

    bpf_probe_read(&transport_header, sizeof(transport_header), &skb->transport_header);
    bpf_probe_read(&head, sizeof(head), &skb->head);
    bpf_probe_read(ih, sizeof(*ih), head + transport_header);
}

__always_inline int get_icmphdr_with_l3(struct sk_buff *skb, struct icmphdr *ich)
{
    struct iphdr ih = {0};
    u16 network_header;
    char *head;
    bpf_probe_read(&network_header, sizeof(network_header), &skb->network_header);
    bpf_probe_read(&head, sizeof(head), &skb->head);
    bpf_probe_read(&ih, sizeof(ih), head + network_header);

    if (ih.protocol == IPPROTO_ICMP)
    {
        bpf_probe_read(ich, sizeof(*ich), head + network_header + (ih.ihl << 2));
        return 0;
    }
    return -1;
}

#if 0
#define MAC_HEADER_SIZE 14
__always_inline int get_icmphdr_with_l2(struct sk_buff *skb, struct icmphdr *ich)
{
    struct iphdr ih = {0};
    u16 mac_header, network_header;
    char *head;
    bpf_probe_read(&mac_header, sizeof(mac_header), &skb->mac_header);
    network_header = mac_header + MAC_HEADER_SIZE;
    bpf_probe_read(&head, sizeof(head), &skb->head);
    bpf_probe_read(&ih, sizeof(ih), head + network_header);

    if (ih.protocol == IPPROTO_ICMP)
    {
        bpf_probe_read(ich, sizeof(*ich), head + network_header + (ih.ihl << 2));
        return 0;
    }
}
#endif

__always_inline void trace_icmp_skb_with_l4(void *ctx, struct sock *sk, struct sk_buff *skb, int type)
{
    struct icmphdr ih = {0};
    struct icmp_event ie = {0};

    if (sk)
    {
        u16 protocol;
        protocol = bpf_core_sock_sk_protocol(sk);
        if (protocol != IPPROTO_ICMP)
            return;
    }

    get_icmphdr_with_l4(skb, &ih);

    ie.skb_ts = bpf_core_skb_tstamp(skb);
    ie.seq = bpf_ntohs(ih.un.echo.sequence);
    ie.id = bpf_ntohs(ih.un.echo.id);
    ie.icmp_type = ih.type; 
    ie.ts = bpf_ktime_get_ns();
    ie.type = type;
    ie.pid = pid();
    COMM(ie.comm);
    
    bpf_perf_event_output(ctx, &perf_map, BPF_F_CURRENT_CPU, &ie, sizeof(ie));
}

// have network header
__always_inline void trace_icmp_skb_with_l3(void *ctx, struct sock *sk, struct sk_buff *skb, int type)
{
    u16 protocol;
    struct icmphdr icmph = {0};
    struct icmp_event ie = {0};

    if (sk)
    {
        protocol = bpf_core_sock_sk_protocol(sk);
        if (protocol != IPPROTO_ICMP)
            return;
    }

    if (get_icmphdr_with_l3(skb, &icmph))
        return;

    ie.skb_ts = bpf_core_skb_tstamp(skb);
    ie.seq = bpf_ntohs(icmph.un.echo.sequence);
    ie.id = bpf_ntohs(icmph.un.echo.id);
    ie.icmp_type = icmph.type;
    ie.ts = bpf_ktime_get_ns();
    ie.type = type;
    ie.pid = pid();
    COMM(ie.comm);
    bpf_perf_event_output(ctx, &perf_map, BPF_F_CURRENT_CPU, &ie, sizeof(ie));
}

#if 0
// no need this function, see verfication/skb_network_header.bt file for more details
__always_inline void trace_icmp_skb_with_l2(void *ctx, struct sock *sk, struct sk_buff *skb, int type)
{
    struct icmphdr icmph = {};
    struct icmp_event ie = {};
    
    if (get_icmphdr_with_l2(skb, &icmph))
        return;

    ie.skb_ts = fix_get_skb_tstamp(skb);
    ie.seq = bpf_ntohs(icmph.un.echo.sequence);
    ie.id = bpf_ntohs(icmph.un.echo.id);
    ie.icmp_type = icmph.type;
    ie.ts = bpf_ktime_get_ns();
    ie.type = type;

    bpf_perf_event_output(ctx, &perf_map, BPF_F_CURRENT_CPU, &ie, sizeof(ie));
}
#endif

__always_inline void raw_and_dgram_entry(void *ctx, struct sock *sk, struct msghdr *msg, bool inet)
{
    struct icmphdr ih = {};
    struct icmp_event ie = {};
    char *ptr;
    u16 protocol;

    protocol = bpf_core_sock_sk_protocol(sk);
    if (protocol != IPPROTO_ICMP)
        return;

    ptr = fix_msghdr_base(msg);
    bpf_probe_read(&ih, sizeof(ih), ptr);

    if (ih.code == 0)
    {
        if (!inet)
            ie.id = ih.un.echo.id;
        else
        {
            struct inet_sock *inetsk = sk;
            bpf_probe_read(&ie.id, sizeof(ie.id), &inetsk->inet_sport);
        }
        ie.id = bpf_ntohs(ie.id);
        ie.seq = bpf_ntohs(ih.un.echo.sequence);

        ie.icmp_type = ih.type;
        ie.ts = bpf_ktime_get_ns();
        ie.type = PING_SND;
        ie.pid = pid();
        COMM(ie.comm);
        bpf_perf_event_output(ctx, &perf_map, BPF_F_CURRENT_CPU, &ie, sizeof(ie));
    }
}

SEC("kprobe/raw_sendmsg")
int BPF_KPROBE(kprobe__raw_sendmsg, u64 arg1, u64 arg2, u64 arg3)
{
    struct sock *sk = NULL;
    struct msghdr *msg = NULL;
    struct msghdr___310 msg310 = {};

    if (!bpf_core_field_exists(msg310.msg_iov)) // alinux2 & 3
    {
        sk = (struct sk_buff *)arg1;
        msg = (struct msghdr *)arg2;
    }
    else // centos 310
    {
        sk = (struct sk_buff *)arg2;
        msg = (struct msghdr *)arg3;
    }
    raw_and_dgram_entry(ctx, sk, msg, false);
    return 0;
}

// ping program can automatic privilege escalation, thus it always is raw socket, even not dgram at non-root user.
SEC("kprobe/ping_v4_sendmsg")
int BPF_KPROBE(kprobe__ping_v4_sendmsg, struct sock *sk, struct msghdr *msg)
{
    raw_and_dgram_entry(ctx, sk, msg, true);
    return 0;
}
#if 0
SEC("kprobe/ping_sendmsg")
int BPF_KPROBE(kprobe__ping_sendmsg, u64 arg1, struct sock *sk, struct msghdr *msg)
{
    raw_and_dgram_entry(ctx, sk, msg, true);
    return 0;
}
#endif

struct tracepoint_args
{
    u32 pad[2];
    struct sk_buff *skbaddr;
};

SEC("tracepoint/net/net_dev_queue")
int tp_net_dev_queue(struct tracepoint_args *args)
{
    struct sk_buff *skb = args->skbaddr;
    struct sock *sk;
    bpf_probe_read(&sk, sizeof(sk), &skb->sk);
    trace_icmp_skb_with_l3(args, sk, skb, PING_NET_DEV_QUEUE);
    return 0;
}

SEC("tracepoint/net/net_dev_xmit")
int tp_net_dev_xmit(struct tracepoint_args *args)
{
    struct sk_buff *skb = args->skbaddr;
    struct sock *sk;
    bpf_probe_read(&sk, sizeof(sk), &skb->sk);
    trace_icmp_skb_with_l3(args, sk, skb, PING_NET_DEV_XMIT);
    return 0;
}

SEC("kprobe/ping_rcv")
int BPF_KPROBE(kprobe__ping_rcv, struct sk_buff *skb)
{
    trace_icmp_skb_with_l4(ctx, NULL, skb, PING_RCV);
    return 0;
}

SEC("kprobe/icmp_rcv")
int BPF_KPROBE(kprobe__icmp_rcv, struct sk_buff *skb)
{
    trace_icmp_skb_with_l4(ctx, NULL, skb, PING_ICMP_RCV);
    return 0;
}

SEC("tracepoint/net/netif_receive_skb")
int tp_netif_receive_skb(struct tracepoint_args *args)
{
    trace_icmp_skb_with_l3(args, NULL, args->skbaddr, PING_NETIF_RCV);
    return 0;
}

#if 0
SEC("kprobe/__kfree_skb")
int BPF_KPROBE(__kfree_skb, struct sk_buff *skb)
{
    struct sock *sk;
    bpf_probe_read(&sk, sizeof(sk), &skb->sk);
    if (sk)
        trace_icmp_skb_with_l4(ctx, sk, skb, PING_KFREE_SKB);
    return 0;
}
#endif 

char _license[] SEC("license") = "GPL";
