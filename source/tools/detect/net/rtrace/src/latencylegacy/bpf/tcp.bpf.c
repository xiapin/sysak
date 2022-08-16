#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_endian.h>

#include "rtrace.h"

struct
{
    __uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
    __uint(key_size, sizeof(u32));
    __uint(value_size, sizeof(u32));
} events SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, struct filter);
} filter_map SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 102400);
    __type(key, struct sk_buff *);
    __type(value, struct sock *);
} skb_sk_map SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 102400);
    __type(key, struct sock *);
    __type(value, struct address_info);
} listend_socks SEC(".maps");

#if 0
struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 102400);
    __type(key, struct skb *);
    __type(value, struct event);
} skb_map SEC(".maps");
#endif

void __always_inline set_addr_pair_by_sock(struct sock *sk, struct addr_pair *ap)
{
    bpf_probe_read(&ap->daddr, sizeof(ap->daddr), &sk->__sk_common.skc_daddr);
    bpf_probe_read(&ap->dport, sizeof(ap->dport), &sk->__sk_common.skc_dport);
    bpf_probe_read(&ap->saddr, sizeof(ap->saddr), &sk->__sk_common.skc_rcv_saddr);
    bpf_probe_read(&ap->sport, sizeof(ap->sport), &sk->__sk_common.skc_num);
    ap->dport = bpf_ntohs(ap->dport);
}

__always_inline int event_sock_filter(struct sock *sk)
{
    __u32 key = 0;
    __u16 protocol;
    struct filter *filter = NULL;
    struct address_info ai = {0};

    if (bpf_map_lookup_elem(&listend_socks, &sk))
        return 0;

    filter = bpf_map_lookup_elem(&filter_map, &key);
    if (!filter)
        goto no_filter;

    ai.pi.pid = bpf_get_current_pid_tgid() >> 32;
    protocol = bpf_core_sock_sk_protocol(sk);
    if (filter->protocol && filter->protocol != protocol)
        return -1;

    if (filter->pid && filter->pid != ai.pi.pid)
        return -1;

    set_addr_pair_by_sock(sk, &ai.ap);
    if (filter->ap.daddr && ai.ap.daddr != filter->ap.daddr)
        return -1;
    if (filter->ap.saddr && ai.ap.saddr != filter->ap.saddr)
        return -1;
    if (filter->ap.dport && ai.ap.dport != filter->ap.dport)
        return -1;
    if (filter->ap.sport && ai.ap.sport != filter->ap.sport)
        return -1;

    goto add_listend_socks;

no_filter:
    ai.pi.pid = bpf_get_current_pid_tgid() >> 32;
    set_addr_pair_by_sock(sk, &ai.ap);

add_listend_socks:
    bpf_get_current_comm(ai.pi.comm, sizeof(ai.pi.comm));
    bpf_map_update_elem(&listend_socks, &sk, &ai, BPF_ANY);
    return 0;
}

#if 0
// Some functions may not have tcp headers, such as __tcp_transmit_skb,
// so seq needs to be obtained from tcp_skb_cb.
static void set_seq_by_tsc(struct sk_buff *skb, uint32_t *seq, uint32_t *end_seq)
{
    struct tcp_skb_cb *tsc;
    tsc = (struct tcp_skb_cb *)((unsigned long)skb + offsetof(struct sk_buff, cb[0]));
    bpf_probe_read(seq, sizeof(*seq), &tsc->seq);
    bpf_probe_read(end_seq, sizeof(*end_seq), &tsc->end_seq);
}

void __always_inline set_tx_seq_by_l4(struct sk_buff *skb, u32 *seq, u32 *end_seq)
{
    u16 transport_header;
    char *head, *data;
    struct tcphdr th = {0};
    u32 len;

    bpf_probe_read(&transport_header, sizeof(transport_header), &skb->transport_header);
    bpf_probe_read(&head, sizeof(head), &skb->head);
    bpf_probe_read(&data, sizeof(data), &skb->data);

    bpf_probe_read(&th, sizeof(th), head + transport_header);
    bpf_probe_read(&len, sizeof(len), &skb->len);

    *seq = th.seq;
    *end_seq = *seq + len - transport_header + (data - head) - th.doff * 4;
}
#endif

__always_inline int get_tcphdr_with_l4(struct sk_buff *skb, struct tcphdr *th)
{
    char *head, *data;
    u16 transport_header;
    u32 len;

    bpf_probe_read(&transport_header, sizeof(transport_header), &skb->transport_header);

    if (transport_header == 0 || transport_header == 0xffff)
        return -1;

    bpf_probe_read(&head, sizeof(head), &skb->head);
    bpf_probe_read(th, sizeof(*th), head + transport_header);
    bpf_probe_read(&data, sizeof(data), &skb->data);
    bpf_probe_read(&len, sizeof(len), &skb->len);

    return len - transport_header + (data - head) - th->doff * 4;
}

__always_inline int trace_skb_with_tsc(void *ctx, struct sock *sk, struct sk_buff *skb, u8 type)
{
    struct tcp_event te = {};
    if (sk && event_sock_filter(sk))
        return -1;

    te.type = type;
    te.sockaddr = (u64)sk;
    te.skbaddr = (u64)skb;

    te.ts = bpf_ktime_get_ns();
    te.skbts = fix_get_skb_tstamp(skb);

    struct tcp_sock *tp = (struct tcp_sock *)sk;
    struct tcp_skb_cb *tsc;
    tsc = (struct tcp_skb_cb *)((unsigned long)skb + offsetof(struct sk_buff, cb[0]));
    bpf_probe_read(&te.seq, sizeof(te.seq), &tsc->seq);
    bpf_probe_read(&te.end_seq, sizeof(te.end_seq), &tsc->end_seq);

    if (type == TCP_ACK)
    {
        bpf_probe_read(&te.snd_una, sizeof(te.snd_una), &tp->snd_una);
        bpf_probe_read(&te.ack_seq, sizeof(te.ack_seq), &tsc->ack_seq);
    }

    bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU, &te, sizeof(te));
    return 0;
}

SEC("kprobe/tcp_sendmsg")
int BPF_KPROBE(kprobe__tcp_sendmsg, struct sock *sk, struct msghdr *msg, size_t size)
{
    if (event_sock_filter(sk))
        return 0;

    struct tcp_event te = {};
    struct tcp_sock *tp = (void *)sk;
    te.type = TCP_SEND_MSG;
    te.sockaddr = (u64)sk;
    te.ts = bpf_ktime_get_ns();

    bpf_probe_read(&te.seq, sizeof(te.seq), &tp->write_seq);
    te.end_seq = te.seq + size;

    bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU, &te, sizeof(te));
    return 0;
}

// SEC("kprobe/__tcp_transmit_skb")
// int BPF_KPROBE(kprobe____tcp_transmit_skb, struct sock *sk, struct sk_buff *skb)
// {
//     trace_skb_with_tsc(ctx, sk, skb, TCP_TRANSMIT_SKB);
//     return 0;
// }

void __always_inline handle_ip_queue_xmit(void *ctx, struct sock *sk, struct sk_buff *skb)
{
    struct tcphdr th = {};
    struct tcp_event te = {};
    u32 data_len = 0;
    u16 protocol;

    if (!sk || event_sock_filter(sk))
        return;

    protocol = bpf_core_sock_sk_protocol(sk);
    if (protocol != IPPROTO_TCP)
        return;

    data_len = get_tcphdr_with_l4(skb, &th);
    if (data_len < 0)
        return;

    te.type = TCP_IP_QUEUE_XMIT;
    te.sockaddr = (u64)sk;
    te.skbaddr = (u64)skb;
    te.ts = bpf_ktime_get_ns();
    te.seq = bpf_ntohl(th.seq);
    te.end_seq = te.seq + data_len;

    bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU, &te, sizeof(te));
    bpf_map_update_elem(&skb_sk_map, &skb, &sk, BPF_ANY);
}

SEC("kprobe/__ip_queue_xmit")
int BPF_KPROBE(kprobe______ip_queue_xmit, struct sock *sk, struct sk_buff *skb)
{
    handle_ip_queue_xmit(ctx, sk, skb);
    return 0;
}

SEC("kprobe/ip_queue_xmit")
int BPF_KPROBE(kprobe__ip_queue_xmit, struct sock *sk, struct sk_buff *skb)
{
    handle_ip_queue_xmit(ctx, sk, skb);
    return 0;
}

struct tracepoint_args
{
    u32 pad[2];
    struct sk_buff *skbaddr;
};

SEC("tracepoint/net/net_dev_xmit")
int tp_net_dev_start_xmit(struct tracepoint_args *args)
{
    struct sk_buff *skb = args->skbaddr;
    struct sock *sk;
    struct sock **skp;
    struct event *event;
    struct tcphdr th = {};
    struct tcp_event te = {};
    u32 data_len = 0;
    u16 protocol;

    bpf_probe_read(&sk, sizeof(sk), &skb->sk);
    if (!sk)
    {
        skp = bpf_map_lookup_elem(&skb_sk_map, &skb);
        if (skp)
            sk = *skp;
    }

    if (!sk || event_sock_filter(sk))
        return 0;
    
    protocol = bpf_core_sock_sk_protocol(sk);
    if (protocol != IPPROTO_TCP)
        return 0;

    data_len = get_tcphdr_with_l4(skb, &th);

    te.type = TCP_NET_DEV_XMIT;
    te.sockaddr = (u64)sk;
    te.skbaddr = (u64)skb;
    te.ts = bpf_ktime_get_ns();
    te.seq = bpf_ntohl(th.seq);
    te.end_seq = te.seq + data_len;

    bpf_perf_event_output(args, &events, BPF_F_CURRENT_CPU, &te, sizeof(te));
    return 0;
}
SEC("kprobe/tcp_ack")
int BPF_KPROBE(kprobe__tcp_ack, struct sock *sk, const struct sk_buff *skb, int flag)
{
    trace_skb_with_tsc(ctx, sk, skb, TCP_ACK);
    return 0;
}

SEC("kprobe/tcp_queue_rcv")
int BPF_KPROBE(kprobe__tcp_queue_rcv, struct sock *sk, struct sk_buff *skb)
{
    trace_skb_with_tsc(ctx, sk, skb, TCP_QUEUE_RCV);
    return 0;
}

SEC("kprobe/tcp_cleanup_rbuf")
int BPF_KPROBE(tcp_cleanup_rbuf, struct sock *sk, int copied)
{
    struct tcp_event te = {};
    struct tcp_sock *tp = (struct tcp_sock *)sk;

    if (event_sock_filter(sk))
        return 0;

    te.type = TCP_CLEANUP_RBUF;

    te.sockaddr = sk;
    te.ts = bpf_ktime_get_ns();

    bpf_probe_read(&te.end_seq, sizeof(te.end_seq), &tp->copied_seq);
    te.seq = te.end_seq - copied;

    bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU, &te, sizeof(te));
    return 0;
}

SEC("kprobe/tcp_recvmsg")
int BPF_KPROBE(kprobe__tcp_recvmsg, struct sock *sk)
{
    event_sock_filter(sk);
    return 0;
}

SEC("kprobe/__kfree_skb")
int BPF_KPROBE(__kfree_skb, struct sk_buff *skb)
{
    struct sock **sk;

    sk = bpf_map_lookup_elem(&skb_sk_map, &skb);
    if (sk != NULL)
        bpf_map_delete_elem(&skb_sk_map, &skb);
    return 0;
}

// trace tcp sock
struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, __u64);
    __type(value, struct sock *);
    __uint(max_entries, 1024);
} tcp_conns SEC(".maps");

// references: https://github.com/iovisor/bcc/blob/master/libbpf-tools/tcpconnect.bpf.c
// client issue connnet.
SEC("kprobe/tcp_v4_connect")
int BPF_KPROBE(kprobe_tcp_v4_connect, struct sock *sk, struct sockaddr *uaddr, int addr_len)
{
    __u64 pid_tgid = bpf_get_current_pid_tgid();
    bpf_map_update_elem(&tcp_conns, &pid_tgid, &sk, BPF_ANY);
    return 0;
}

// client has build the connnet and into established state.
SEC("kretprobe/tcp_v4_connect")
int BPF_KRETPROBE(kretprobe_tcp_v4_connect, int ret)
{
    __u64 pid_tgid;
    struct sock **skp;
    struct addr_pair ap = {};
    pid_tgid = bpf_get_current_pid_tgid();
    if (ret != 0)
        goto exit;

    skp = bpf_map_lookup_elem(&tcp_conns, &pid_tgid);
    if (!skp)
        goto exit;

    event_sock_filter(*skp);
exit:
    bpf_map_delete_elem(&tcp_conns, &pid_tgid);
    return 0;
}

// ref: https://github.com/iovisor/bcc/blob/e83019bdf6c400b589e69c7d18092e38088f89a8/tools/tcpaccept.py
// server has build the connect and into established state.
SEC("kretprobe/inet_csk_accept")
int BPF_KRETPROBE(kretprobe_inet_csk_accept, struct sock *sk)
{
    struct addr_pair ap = {};
    struct inet_sock *inet;
    if (!sk)
        return 0;

    event_sock_filter(sk);
    return 0;
}

// destroy the connect.
SEC("kprobe/tcp_v4_destroy_sock")
int BPF_KPROBE(kprobe_tcp_v4_destroy_sock, struct sock *sk)
{
#if 0
    bpf_printk("%d\n", offsetof(struct address_info, ap));
#endif
    bpf_map_delete_elem(&listend_socks, &sk);
    return 0;
}

char LICENSE[] SEC("license") = "GPL";