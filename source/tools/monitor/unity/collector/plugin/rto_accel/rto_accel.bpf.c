//
// Created by 廖肇燕 on 2023/8/1.
//
#include <vmlinux.h>
#include <coolbpf.h>

#ifndef __section
# define __section(NAME)                  \
  __attribute__((section(NAME), used))
#endif

#define SOL_TCP 6

__section("sockops")
int set_rto_min(struct bpf_sock_ops *skops)
{
    int op = (int)skops->op;
    int rv = -1;
    if (op == BPF_SOCK_OPS_ACTIVE_ESTABLISHED_CB) {
        int rto_min_usecs = 8000;
        rv = bpf_setsockopt(skops, SOL_TCP, TCP_BPF_RTO_MIN, &rto_min_usecs, sizeof(rto_min_usecs));
    }
    skops->reply = rv;
    return 1;
}
