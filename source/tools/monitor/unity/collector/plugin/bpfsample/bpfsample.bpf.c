

#include <vmlinux.h>
#include <coolbpf.h>
#include "bpfsample.h"



BPF_ARRAY(count, u64, 200);

SEC("kprobe/netstat_seq_show")
int BPF_KPROBE(netstat_seq_show, struct sock *sk, struct msghdr *msg, size_t size)
{
    int default_key = 0;
    u64 *value = bpf_map_lookup_elem(&count, &default_key);
    if (value) {
        __sync_fetch_and_add(value, 1);
    }
    return 0;
}



