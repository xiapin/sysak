#define BPF_NO_GLOBAL_DATA
#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_endian.h>

#include "bpf_core.h"

#include "sli.h"

struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, u32);
    __type(value, struct latency_hist);
} latency_map SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
    __uint(key_size, sizeof(u32));
    __uint(value_size, sizeof(u32));
} events SEC(".maps");

void __always_inline set_addr_pair_by_sock(struct sock *sk, struct addr_pair *ap)
{
    bpf_probe_read(&ap->daddr, sizeof(ap->daddr), &sk->__sk_common.skc_daddr);
    bpf_probe_read(&ap->dport, sizeof(ap->dport), &sk->__sk_common.skc_dport);
    bpf_probe_read(&ap->saddr, sizeof(ap->saddr), &sk->__sk_common.skc_rcv_saddr);
    bpf_probe_read(&ap->sport, sizeof(ap->sport), &sk->__sk_common.skc_num);
    ap->dport = bpf_ntohs(ap->dport);
}

SEC("kprobe/tcp_ack")
int BPF_KPROBE(kprobe__tcp_ack, struct sock *sk, const struct sk_buff *skb, int flag)
{
    u32 srtt;
    u32 key = 0;
    struct tcp_sock *tp;
    struct latency_hist *lhp;

    tp = (struct tcp_sock *)sk;
    bpf_probe_read(&srtt, sizeof(srtt), &tp->srtt_us);
    srtt >>= 3;
    srtt /= 1000;
    srtt >>= 3;

    lhp = bpf_map_lookup_elem(&latency_map, &key);
    if (lhp)
    {
        if (srtt < MAX_LATENCY_SLOTS)
            lhp->latency[srtt]++;
        else
        {
            lhp->overflow++;
            struct event e = {};
            set_addr_pair_by_sock(sk, &e.le.ap);
            e.le.latency = srtt;
            bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU, &e, sizeof(e));
        }
    }

    return 0;
}