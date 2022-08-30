#define BPF_NO_GLOBAL_DATA
#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_endian.h>

#include "common.h"
#include "bpf_core.h"
#include "connectlatency.h"

BPF_HASH(sockmap, u64, struct sockmap_val, 10240);
BPF_HASH(inner_tidmap, u32, struct sock *, 10240);

static __always_inline void fill_ts(struct sockmap_val *val, u32 type)
{
    u32 idx = val->curidx;
    if (idx < MAX_EVENT_NUM)
    {
        val->tss[idx].event = type;
        val->tss[idx].ts = ns();
        val->curidx = idx + 1;
        bpf_printk("idx: %u, type: %u\n", idx + 1, type);
    }
}

SEC("kprobe/tcp_v4_connect")
int BPF_KPROBE(tcp_v4_connect, struct sock *sk)
{
    int key = 0;
    struct filter *filter;
    struct sockmap_val val = {0};
    u32 pid = pid();
    u32 tid = tid();

    filter = bpf_map_lookup_elem(&filter_map, &key);
    if (filter && filter->pid && filter->pid != pid)
        return 0;

    val.pid = pid;
    comm(val.comm);
    val.curidx = 0;
    fill_ts(&val, TCP_CONNECT);
    bpf_map_update_elem(&sockmap, &sk, &val, BPF_ANY);
    bpf_map_update_elem(&inner_tidmap, &tid, &sk, BPF_ANY);
    return 0;
}

SEC("kprobe/tcp_rcv_state_process")
int BPF_KPROBE(tcp_rcv_state_process, struct sock *sk)
{
    struct sockmap_val *val;
    u8 state;
    // u8 state = READ_KERN(sk->__sk_common.skc_state);
    bpf_probe_read(&state, sizeof(state), &sk->__sk_common.skc_state);
    if (state != TCP_SYN_SENT)
        return 0;

    bpf_printk("tcp_rcv_state_process\n");
    val = bpf_map_lookup_elem(&sockmap, &sk);
    if (!val)
        return 0;

    set_addr_pair_by_sock(sk, &val->ap);
    fill_ts(val, TCP_RCV_SYNACK);
    bpf_perf_event_output(ctx, &perf_map, BPF_F_CURRENT_CPU, val, sizeof(struct sockmap_val));
    bpf_map_delete_elem(&sockmap, &sk);
    return 0;
}

// SEC("kretprobe/tcp_v4_connect")
// int BPF_KRETPROBE(tcp_v4_connect_ret, int ret)
// {
//     struct sock **sk;
//     struct sockmap_val *val;
//     u32 tid = tid();

//     sk = bpf_map_lookup_elem(&inner_tidmap, &tid);
//     if (sk)
//     {
//         val = bpf_map_lookup_elem(&sockmap, sk);
//         if (val)
//         {
//             val->ret = ret;
//             set_addr_pair_by_sock(*sk, &val->ap);
//             fill_ts(val, TCP_CONNECT_RET);
//             // bpf_map_delete_elem(&sockmap, sk);
//         }
//     }

//     return 0;
// }
char _license[] SEC("license") = "GPL";