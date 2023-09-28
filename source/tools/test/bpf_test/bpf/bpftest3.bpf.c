#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

#ifndef __section
# define __section(NAME)                  \
    __attribute__((section(NAME), used))
#endif

#define SOL_TCP 6
__section ("sockops")
int adjust_rto(struct bpf_sock_ops *skops)
{
        int op = (int) skops->op;
        int rv = -1;
        if (op == BPF_SOCK_OPS_TIMEOUT_INIT) {
	        bpf_printk("set init rto: done\n");
	        rv = 2; // 2 jiffies
        }
        else if (op == BPF_SOCK_OPS_TCP_CONNECT_CB) {
                bpf_sock_ops_cb_flags_set(skops, BPF_SOCK_OPS_RTO_CB_FLAG);
                bpf_printk("set rto flag: done\n");
                int rto_min_usecs = 8000;
                rv = bpf_setsockopt(skops, SOL_TCP, TCP_BPF_RTO_MIN, &rto_min_usecs, sizeof(rto_min_usecs));
                bpf_printk("set min rto: %d\n", rv);
                return 1;
        }
        else if (op == BPF_SOCK_OPS_RTO_CB) {
                int current_rto = skops->args[1];
                bpf_printk("current rto: %d\n", current_rto);
                rv = 1;
        }
        skops->reply = rv;
        return 1;
}

char _license[] __section("license") = "GPL";
