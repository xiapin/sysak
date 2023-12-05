#define BPF_NO_GLOBAL_DATA
#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_endian.h>

#include "pingtrace.h"

struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, struct ping_key);
    __type(value, struct ping_sender);
    __uint(max_entries, 1024);
} ping_events SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, u64);
    __type(value, void *);
    __uint(max_entries, 1024);
} tid_msghdr SEC(".maps");

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

__always_inline bool l4_set_ping_key(struct sk_buff *skb, struct ping_key *key, int ty)
{
    struct icmphdr icmph = {0};

    get_icmphdr_with_l4(skb, &icmph);
    if (ty != icmph.type)
        return false;

    key->seq = bpf_ntohs(icmph.un.echo.sequence);
    key->id = bpf_ntohs(icmph.un.echo.id);
    return true;
}

__always_inline bool l3_set_ping_key(struct sk_buff *skb, struct ping_key *key, int ty)
{
    struct icmphdr icmph = {0};

    if (get_icmphdr_with_l3(skb, &icmph) != 0)
        return false;

    if (ty != icmph.type)
        return false;

    key->seq = bpf_ntohs(icmph.un.echo.sequence);
    key->id = bpf_ntohs(icmph.un.echo.id);
    return true;
}

struct msghdr___310
{
    struct iovec *msg_iov;
};

static __always_inline u16 bpf_core_sock_sk_protocol(struct sock *sk)
{
    return (u16)BPF_CORE_READ_BITFIELD_PROBED(sk, sk_protocol);
}

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

__always_inline void raw_and_dgram_entry(void *ctx, struct sock *sk, struct msghdr *msg, bool inet)
{
    struct icmphdr ih = {};
    struct ping_key key = {0};
    char *ptr;
    u16 protocol;
    struct ping_sender sender = {0};

    protocol = bpf_core_sock_sk_protocol(sk);
    if (protocol != IPPROTO_ICMP)
        return;

    ptr = fix_msghdr_base(msg);
    bpf_probe_read(&ih, sizeof(ih), ptr);

    if (ih.code == 0)
    {
        if (!inet)
            key.id = ih.un.echo.id;
        else
        {
            struct inet_sock *inetsk = sk;
            bpf_probe_read(&key.id, sizeof(key.id), &inetsk->inet_sport);
        }
        key.id = bpf_ntohs(key.id);
        key.seq = bpf_ntohs(ih.un.echo.sequence);
        sender.ty = PING;
        sender.stages[PING_SND].ts = bpf_ktime_get_ns();
        sender.stages[PING_SND].cpu = bpf_get_smp_processor_id();
        bpf_map_update_elem(&ping_events, &key, &sender, BPF_ANY);
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

struct tp_net_arg
{
    u32 pad[2];
    struct sk_buff *skbaddr;
};

SEC("tracepoint/net/net_dev_queue")
int tp_net_dev_queue(struct tp_net_arg *args)
{
    struct sk_buff *skb = args->skbaddr;
    struct ping_key key = {0};
    if (!l3_set_ping_key(skb, &key, ICMP_ECHO))
        return 0;

    struct ping_sender *sender = bpf_map_lookup_elem(&ping_events, &key);
    if (!sender)
        return 0;

    sender->stages[PING_DEV_QUEUE].ts = bpf_ktime_get_ns();
    sender->stages[PING_DEV_QUEUE].cpu = bpf_get_smp_processor_id();
    return 0;
}

SEC("tracepoint/net/net_dev_xmit")
int tp_net_dev_xmit(struct tp_net_arg *args)
{
    struct sk_buff *skb = args->skbaddr;
    struct ping_key key = {0};
    if (!l3_set_ping_key(skb, &key, ICMP_ECHO))
        return 0;

    struct ping_sender *sender = bpf_map_lookup_elem(&ping_events, &key);
    if (!sender)
        return 0;

    sender->stages[PING_DEV_XMIT].ts = bpf_ktime_get_ns();
    sender->stages[PING_DEV_XMIT].cpu = bpf_get_smp_processor_id();
    return 0;
}

SEC("tracepoint/net/netif_receive_skb")
int tp_netif_receive_skb(struct tp_net_arg *args)
{
    struct sk_buff *skb = args->skbaddr;
    struct ping_key key = {0};
    if (!l3_set_ping_key(skb, &key, ICMP_ECHOREPLY))
        return 0;

    struct ping_sender *sender = bpf_map_lookup_elem(&ping_events, &key);
    if (!sender)
        return 0;

    int cpu = bpf_get_smp_processor_id();
    output_all_events(args, cpu);
    sender->stages[PING_NETIF_RCV].ts = bpf_ktime_get_ns();
    sender->stages[PING_NETIF_RCV].cpu = cpu;
    return 0;
}

SEC("kprobe/icmp_rcv")
int BPF_KPROBE(kprobe__icmp_rcv, struct sk_buff *skb)
{
    struct ping_key key = {0};
    if (!l4_set_ping_key(skb, &key, ICMP_ECHOREPLY))
        return 0;

    struct ping_sender *sender = bpf_map_lookup_elem(&ping_events, &key);
    if (!sender)
        return 0;

    sender->stages[PING_ICMP_RCV].ts = bpf_ktime_get_ns();
    sender->stages[PING_ICMP_RCV].cpu = bpf_get_smp_processor_id();
    return 0;
}

#if 0
SEC("kprobe/ping_rcv")
int BPF_KPROBE(kprobe__ping_rcv, struct sk_buff *skb)
{
    struct ping_key key = {0};
    int cpu = bpf_get_smp_processor_id();
    if (!l4_set_ping_key(skb, &key, ICMP_ECHOREPLY))
        return 0;

    struct ping_sender *sender = bpf_map_lookup_elem(&ping_events, &key);
    if (!sender)
        return 0;

    sender->key = key;
    sender->stages[PING_RCV].ts = bpf_ktime_get_ns();
    sender->stages[PING_RCV].cpu = cpu;
    bpf_perf_event_output(ctx, &perf_events, BPF_F_CURRENT_CPU, sender, sizeof(*sender));
    return 0;
}
#endif

SEC("kprobe/skb_free_datagram")
int BPF_KPROBE(kprobe__skb_free_datagram, struct sock *sk, struct sk_buff *skb)
{
    struct ping_key key = {0};
    int cpu = bpf_get_smp_processor_id();
    u16 protocol = bpf_core_sock_sk_protocol(sk);
    if (protocol != IPPROTO_ICMP)
        return 0;
    if (!l4_set_ping_key(skb, &key, ICMP_ECHOREPLY))
        return 0;

    struct ping_sender *sender = bpf_map_lookup_elem(&ping_events, &key);
    if (!sender)
        return 0;

    sender->key = key;
    sender->stages[PING_RCV].ts = bpf_ktime_get_ns();
    sender->stages[PING_RCV].cpu = cpu;
    bpf_perf_event_output(ctx, &perf_events, BPF_F_CURRENT_CPU, sender, sizeof(*sender));
    return 0;
}

char _license[] SEC("license") = "GPL";
