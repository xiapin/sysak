from pylcc import ClbcBase
import time

bpfPog = r"""
#include "lbc.h"

LBC_ARRAY(outCnt, int, u64, 2);
LBC_HIST10(netHist);

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
        hist10_push(&netHist, ms);
    }
    return 0;
}

char _license[] SEC("license") = "GPL";
"""


class CnetHealth(ClbcBase):
    def __init__(self):
        super(CnetHealth, self).__init__("net_health_bpf", bpf_str=bpfPog)

    def loop(self):
        while True:
            time.sleep(20)
            print(self.maps['outCnt'].get())
            print(self.maps['netHist'].get())


if __name__ == "__main__":
    e = CnetHealth()
    e.loop()
