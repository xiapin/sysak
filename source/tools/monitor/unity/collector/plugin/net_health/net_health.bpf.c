//
// Created by 廖肇燕 on 2023/2/24.
//
#include <vmlinux.h>
#include <coolbpf.h>

BPF_ARRAY(outCnt, u64, 2);
BPF_ARRAY(netHist, u64, 20);

static inline void addCnt(int k, u64 val) {
    u64 *pv = bpf_map_lookup_elem(&outCnt, &k);
    if (pv) {
        __sync_fetch_and_add(pv, val);
    }
}

SEC("kprobe/tcp_validate_incoming")
int j_tcp_validate_incoming(struct pt_regs *ctx) {
    struct tcp_sock *tp = (struct tcp_sock *)PT_REGS_PARM1(ctx);
    u64 ts = BPF_CORE_READ(tp, srtt_us) >> 3;
    u64 ms = ts / 1000;
    if (ms > 0) {
        addCnt(0, ms);
        addCnt(1, 1);
        hist10_push((struct bpf_map_def *)&netHist, ms);
    }
    return 0;
}

