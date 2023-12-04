#define BPF_NO_GLOBAL_DATA
#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_endian.h>

#include "queueslow.h"

struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 1024000);
    __type(key, u64);
    __type(value, u64);
} skbs SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, u32);
    __type(value, struct filter);
} filters SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
    __uint(key_size, sizeof(u32));
    __uint(value_size, sizeof(u32));
} perf_events SEC(".maps");

struct net_dev_queue_arg
{
    u64 pad;
    void *skb;
};

__always_inline void set_addrpair(struct queue_slow *qs, struct sk_buff *skb)
{

    struct iphdr ih = {};
    struct tcphdr th = {};
    struct udphdr uh = {};
    u16 network_header, transport_header;
    char *head;

    bpf_probe_read(&head, sizeof(head), &skb->head);
    bpf_probe_read(&network_header, sizeof(network_header), &skb->network_header);
    if (network_header != 0)
    {
        bpf_probe_read(&ih, sizeof(ih), head + network_header);
        qs->saddr = ih.saddr;
        qs->daddr = ih.daddr;
        qs->protocol = ih.protocol;
        transport_header = network_header + (ih.ihl << 2);
    }
    switch (qs->protocol)
    {
    case IPPROTO_UDP:
        if (transport_header != 0 && transport_header != 0xffff)
        {
            bpf_probe_read(&uh, sizeof(uh), head + transport_header);
            qs->sport = bpf_ntohs(uh.source);
            qs->dport = bpf_ntohs(uh.dest);
        }
        break;
    case IPPROTO_TCP:
        bpf_probe_read(&th, sizeof(th), head + transport_header);
        qs->sport = bpf_ntohs(th.source);
        qs->dport = bpf_ntohs(th.dest);
        break;
    default:
        break;
    }
}

SEC("tracepoint/net/net_dev_queue")
int tp_net_dev_queue(struct net_dev_queue_arg *arg)
{
    u64 ts = bpf_ktime_get_ns();
    void *skb = arg->skb;
    bpf_map_update_elem(&skbs, &skb, &ts, BPF_ANY);
    return 0;
}

SEC("tracepoint/net/net_dev_xmit")
int tp_net_dev_xmit(struct net_dev_queue_arg *arg)
{
    void *skb = arg->skb;
    u64 *prev_ts = bpf_map_lookup_elem(&skbs, &skb);
    if (!prev_ts)
        return 0;
    int key = 0;
    struct filter *filter = bpf_map_lookup_elem(&filters, &key);
    if (!filter)
        return 0;

    u64 ts = bpf_ktime_get_ns();
    u64 delta = ts - *prev_ts;
    if (delta > filter->threshold)
    {
        struct queue_slow qs = {0};
        set_addrpair(&qs, skb);

        if (qs.protocol != filter->protocol)
            return 0;

        bpf_perf_event_output(arg, &perf_events, BPF_F_CURRENT_CPU, &qs, sizeof(qs));
    }

    return 0;
}

char _license[] SEC("license") = "GPL";