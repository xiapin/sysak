//
// Created by 廖肇燕 on 2023/4/11.
//
#define BPF_NO_GLOBAL_DATA
#include <vmlinux.h>
#include <coolbpf.h>

BPF_HASH(dips,  u32, u64, 1024);
BPF_HASH(inums, u32, u64, 1024);

static inline u32 read_ns_inum(struct sock *sk)
{
    if (sk) {
        return BPF_CORE_READ(sk, __sk_common.skc_net.net, ns.inum);
    }
    return 0;
}

static void inc_value(struct bpf_map* maps, u32 k) {
    u64 *pv = bpf_map_lookup_elem(maps, &k);
    if (pv) {
        __sync_fetch_and_add(pv, 1);
    } else {
        u64 v = 1;
        bpf_map_update_elem(maps, &k, &v, BPF_ANY);
    }
}

struct tcp_retrans_args {
    u64 pad;
    u64 skb;
    u64 sk;
    u16 sport;
    u16 dport;
    u32 sip;
    u32 dip;
};
SEC("tracepoint/tcp/tcp_retransmit_skb")
int tcp_retransmit_skb_hook(struct tcp_retrans_args *args){
    struct sock *sk = (struct sock *)args->sk;
    u32 inum = read_ns_inum(sk);
    u32 ip = args->dip;

    inc_value((struct bpf_map *)&inums, inum);
    inc_value((struct bpf_map *)&dips,  ip);
    return 0;
}
