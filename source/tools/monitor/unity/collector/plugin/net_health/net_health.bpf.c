//
// Created by 廖肇燕 on 2023/2/24.
//
#include <vmlinux.h>
#include <coolbpf.h>

BPF_HASH(outCnt, int, u64, 2);
BPF_ARRAY(netHist, u64, 20);

SEC("kprobe/tcp_validate_incoming")
int j_tcp_validate_incoming(struct pt_regs *ctx) {
    struct tcp_sock *tp = (struct tcp_sock *)PT_REGS_PARM1(ctx);
    u64 ts = BPF_CORE_READ(tp, srtt_us) >> 3;
    u64 ms = ts / 1000;
    if (ms > 0) {
        add_hist((struct bpf_map_def *)&outCnt, 0, ms);
        add_hist((struct bpf_map_def *)&outCnt, 1, 1);
        hist10_push((struct bpf_map_def *)&netHist, ms);
    }
    return 0;
}

