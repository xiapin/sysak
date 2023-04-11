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

static inline u32 read_dip(struct sock *sk)
{
    if (sk) {
        return BPF_CORE_READ(sk, __sk_common.skc_daddr);
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

SEC("kprobe/tcp_retransmit_skb")
int j_tcp_retransmit_skb(struct pt_regs *ctx){
    struct sock *sk;
    u32 inum, ip;

    sk = (struct sock *)PT_REGS_PARM1(ctx);
    inum = read_ns_inum(sk);
    ip = read_dip(sk);

    bpf_printk("hello  inum: %u\n", inum);

    inc_value((struct bpf_map *)&inums, inum);
    inc_value((struct bpf_map *)&dips,  ip);
    return 0;
}
