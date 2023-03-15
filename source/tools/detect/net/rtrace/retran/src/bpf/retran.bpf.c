#define BPF_NO_GLOBAL_DATA
#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_endian.h>

#include "common.h"
#include "bpf_core.h"
#include "retran.h"


struct tracepoint_args
{
    u32 pad[2];
    struct sk_buff *skb;
    struct sock *sk;
};


__always_inline void trace_retransmit(void *ctx, struct sock *sk, struct sk_buff *skb)
{
    struct inet_connection_sock *icsk = sk;
    struct retran_event re = {};

    bpf_probe_read(&re.retran_times, sizeof(icsk->icsk_retransmits), &icsk->icsk_retransmits);
    re.retran_times++;

    bpf_probe_read(&re.tcp_state, sizeof(sk->__sk_common.skc_state), &sk->__sk_common.skc_state);
    re.ca_state = BPF_CORE_READ_BITFIELD_PROBED(icsk, icsk_ca_state);

    set_addr_pair_by_sock(sk, &re.ap);
    re.ts = ns();
    bpf_perf_event_output(ctx, &perf_map, BPF_F_CURRENT_CPU, &re, sizeof(re));
}

SEC("tracepoint/tcp/tcp_retransmit_skb")
int tp_tcp_retransmit_skb(struct tracepoint_args *args) 
{
    struct sk_buff *skb = args->skb;
    struct sock *sk = args->sk;
    trace_retransmit(args, sk, skb);
    return 0;
}


SEC("kprobe/tcp_retransmit_skb")
int BPF_KPROBE(tcp_retransmit_skb, struct sock *sk, struct sk_buff *skb)
{
    trace_retransmit(ctx, sk, skb);
    return 0;
}

char _license[] SEC("license") = "GPL";
