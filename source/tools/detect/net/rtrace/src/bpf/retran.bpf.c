#define BPF_NO_GLOBAL_DATA
#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_endian.h>

#include "retran.h"

struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 102400);
    __type(key, u64);
    __type(value, u8);
} sockmap SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
    __uint(key_size, sizeof(u32));
    __uint(value_size, sizeof(u32));
} perf_events SEC(".maps");


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
    struct tcphdr th = {};
    char *head;
    u16 transport_header;
    u8 *retran_type;

    bpf_probe_read(&re.tcp_state, sizeof(sk->__sk_common.skc_state), &sk->__sk_common.skc_state);
    re.ca_state = BPF_CORE_READ_BITFIELD_PROBED(icsk, icsk_ca_state);
    
    retran_type = bpf_map_lookup_elem(&sockmap, &sk);
    if (retran_type)
    {
        // RTO_RETRAN TLP SYN_RETRAN
        re.retran_type = *retran_type;
        bpf_map_delete_elem(&sockmap, &sk);
    }
    else
    {
        bpf_probe_read(&head, sizeof(head), &skb->head);
        bpf_probe_read(&transport_header, sizeof(transport_header), &skb->transport_header);
        bpf_probe_read(&th, sizeof(th), head + transport_header);
        if (th.syn)
            re.retran_type = SYN_RETRAN;
        else
        {
            if (re.ca_state == 4)
                re.retran_type = SLOW_START_RETRAN;
            else
                re.retran_type = FAST_RETRAN;
        }
    }

    bpf_probe_read(&re.daddr, sizeof(re.daddr), &sk->__sk_common.skc_daddr);
    bpf_probe_read(&re.dport, sizeof(re.dport), &sk->__sk_common.skc_dport);
    bpf_probe_read(&re.saddr, sizeof(re.saddr), &sk->__sk_common.skc_rcv_saddr);
    bpf_probe_read(&re.sport, sizeof(re.sport), &sk->__sk_common.skc_num);
    re.dport = bpf_ntohs(re.dport);
    
    bpf_perf_event_output(ctx, &perf_events, BPF_F_CURRENT_CPU, &re, sizeof(re));
}

#if 0
SEC("tracepoint/tcp/tcp_retransmit_skb")
int tp_tcp_retransmit_skb(struct tracepoint_args *args)
{
    struct sk_buff *skb = args->skb;
    struct sock *sk = args->sk;
    trace_retransmit(args, sk, skb);
    return 0;
}
#endif

SEC("kprobe/__tcp_retransmit_skb")
int BPF_KPROBE(__tcp_retransmit_skb, struct sock *sk, struct sk_buff *skb)
{
    trace_retransmit(ctx, sk, skb);
    return 0;
}

SEC("kprobe/tcp_enter_loss")
int BPF_KPROBE(tcp_enter_loss, struct sock *sk)
{
    u8 retran_type = RTO_RETRAN;
    bpf_map_update_elem(&sockmap, &sk, &retran_type, BPF_ANY);
    return 0;
}

SEC("kprobe/tcp_rtx_synack")
int BPF_KPROBE(tcp_rtx_synack, struct sock *sk, struct request_sock *req)
{
    u8 retran_type = SYN_RETRAN;
    bpf_map_update_elem(&sockmap, &sk, &retran_type, BPF_ANY);
    trace_retransmit(ctx, sk, NULL);
    return 0;
}

SEC("kprobe/tcp_send_loss_probe")
int BPF_KPROBE(tcp_send_loss_probe, struct sock *sk)
{
    u8 retran_type = TLP;
    bpf_map_update_elem(&sockmap, &sk, &retran_type, BPF_ANY);
    return 0;
}

char _license[] SEC("license") = "GPL";
