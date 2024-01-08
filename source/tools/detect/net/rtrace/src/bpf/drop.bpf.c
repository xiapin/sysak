#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_endian.h>

#include "drop.h"

struct
{
    __uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
    __uint(key_size, sizeof(u32));
    __uint(value_size, sizeof(u32));
} perf_events SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, u32);
    __type(value, struct drop_filter);
} filters SEC(".maps");

struct kfree_skb_tp_args
{
    u32 pad[2];
    struct sk_buff *skbaddr;
    u64 location;
    u16 protocol;
};

__always_inline int fill_event(void *ctx, struct drop_event *event, struct sk_buff *skb)
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
        event->saddr = ih.saddr;
        event->daddr = ih.daddr;
        event->proto = ih.protocol;
        transport_header = network_header + (ih.ihl << 2);
    }
    else
    {
        bpf_probe_read(&transport_header, sizeof(transport_header), &skb->transport_header);
    }
    switch (event->proto)
    {
    case IPPROTO_UDP:
        if (transport_header != 0 && transport_header != 0xffff)
        {
            bpf_probe_read(&uh, sizeof(uh), head + transport_header);
            event->sport = bpf_ntohs(uh.source);
            event->dport = bpf_ntohs(uh.dest);
        }
        break;
    case IPPROTO_TCP:
        bpf_probe_read(&th, sizeof(th), head + transport_header);
        event->sport = bpf_ntohs(th.source);
        event->dport = bpf_ntohs(th.dest);
        break;
    default:
        break;
    }
    int key = 0;
    struct drop_filter *filter = bpf_map_lookup_elem(&filters, &key);
    if (!filter)
        return 0;
    if (filter->protocol != event->proto)
        return 0;

    if (filter->saddr && (filter->saddr != event->saddr || filter->daddr != event->daddr))
        return 0;

    if (filter->daddr && (filter->daddr != event->daddr || filter->saddr != event->saddr))
        return 0;

    if (filter->sport && (filter->sport != event->sport || filter->dport != event->dport))
        return 0;

    if (filter->dport && (filter->dport != event->dport || filter->sport != event->sport))
        return 0;

    bpf_perf_event_output(ctx, &perf_events, BPF_F_CURRENT_CPU, event, sizeof(struct drop_event));
    return 0;
}

SEC("tracepoint/skb/kfree_skb")
int tp_kfree_skb(struct kfree_skb_tp_args *ctx)
{
    struct drop_event event = {};

    event.proto = ctx->protocol;
    event.location = ctx->location;
    fill_event(ctx, &event, ctx->skbaddr);
    return 0;
}

SEC("kprobe/tcp_drop")
int BPF_KPROBE(tcp_drop, struct sock *sk, struct sk_buff *skb)
{

    struct drop_event event = {};
    u64 bp;
    bpf_probe_read(&event.proto, sizeof(event.proto), &skb->protocol);
    event.proto = bpf_ntohs(event.proto);
    BPF_KPROBE_READ_RET_IP(bp, ctx);

    bpf_probe_read(&event.location, sizeof(event.location), (void *)(bp + 8));
    fill_event(ctx, &event, skb);
    return 0;
}

char _license[] SEC("license") = "GPL";