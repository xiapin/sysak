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
    __uint(tyoe, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 102400);
    __type(key, u64);
    __type(value, u64);
} sockmap SEC(".maps");

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

__always_inline void handle_rtt(void *ctx, struct sock *sk, u32 rtt)
{
    u32 key = 0;
    struct latency_hist *lhp;

    lhp = bpf_map_lookup_elem(&latency_map, &key);
    if (lhp)
    {
        rtt /= 1000;
        asm volatile("%0 = %1"
                     : "=r"(rtt)
                     : "r"(rtt));

        if (rtt < MAX_LATENCY_SLOTS)
            lhp->latency[rtt]++;

        if (rtt >= lhp->threshold)
        {
            lhp->overflow++;
            struct event e = {};
            e.event_type = LATENCY_EVENT;
            set_addr_pair_by_sock(sk, &e.le.ap);
            e.le.latency = rtt;
            bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU, &e, sizeof(e));
        }
    }
}

SEC("kprobe/tcp_ack")
int BPF_KPROBE(kprobe__tcp_ack, struct sock *sk, const struct sk_buff *skb, int flag)
{
    u32 srtt;
    struct tcp_sock *tp;

    tp = (struct tcp_sock *)sk;
    bpf_probe_read(&srtt, sizeof(srtt), &tp->srtt_us);

    srtt >>= 3;
    handle_rtt(ctx, sk, srtt);

    return 0;
}

SEC("kprobe/tcp_rtt_estimator")
int BPF_KPROBE(kprobe__tcp_rtt_estimator, struct sock *sk, long mrtt_us)
{
    if (mrtt_us > 0)
        handle_rtt(ctx, sk, (u32)mrtt_us);
    return 0;
}

struct tracepoint_args
{
    u32 pad[2];
    struct sock *skaddr;
};

SEC("tracepoint/tcp/tcp_rcv_space_adjust")
int tp__tcp_rcv_space_adjust(struct trace_event_raw_tcp_event_sk *args)
{
    u32 key = 1;
    u64 *pre_ts;
    pre_ts = bpf_map_lookup_elem(&sockmap, &args->sock_cookie);

    if (pre_ts)
    {
        u64 cur_ts = bpf_ktime_get_ns();
        u64 delta = (cur_ts - *pre_ts)/1000000;

        struct latency_hist *lhp;
        lhp = bpf_map_lookup_elem(&latency_map, &key);

        if (lhp)
        {
            if (delta < MAX_LATENCY_SLOTS)
                lhp->latency[delta]++;

            if (delta >= lhp->threshold)
            {
                struct event e = {};
                e.event_type = USR_LATENCY_EVENT;
                e.le.ap.saddr = args->saddr;
                e.le.ap.daddr = args->daddr;
                e.le.ap.sport = args->sport;
                e.le.ap.dport = args->dport;
                e.le.latency = delta;

                bpf_perf_event_output(args, &events, BPF_F_CURRENT_CPU, &e, sizeof(e));
            }
        }
    }
    return 0;
}

SEC("tracepoint/tcp/tcp_probe")
int tp__tcp_probe(struct trace_event_raw_tcp_probe *args)
{
    u64 ts = bpf_ktime_get_ns();
    bpf_map_update_elem(&sockmap, &args->sock_cookie, &ts, 0);
    return 0;
}

char _license[] SEC("license") = "GPL";