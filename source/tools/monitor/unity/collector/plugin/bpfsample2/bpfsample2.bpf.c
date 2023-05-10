

#include <vmlinux.h>
#include <coolbpf.h>
#include "bpfsample2.h"



BPF_PERF_OUTPUT(perf, 1024);


SEC("kprobe/netstat_seq_show")
int BPF_KPROBE(netstat_seq_show, struct sock *sk, struct msghdr *msg, size_t size)
{
    struct event e = {};

    e.ns = ns();
    e.cpu = cpu();
    e.pid = pid();
    comm(e.comm);
    
    bpf_perf_event_output(ctx, &perf, BPF_F_CURRENT_CPU, &e, sizeof(struct event));
    return 0;
}



