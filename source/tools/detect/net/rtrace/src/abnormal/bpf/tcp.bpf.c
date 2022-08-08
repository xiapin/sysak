

#define BPF_NO_GLOBAL_DATA
#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_endian.h>

#include "abnormal.h"
#include "bpf_core.h"

struct
{
    __uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
    __uint(key_size, sizeof(u32));
    __uint(value_size, sizeof(u32));
} events SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, struct filter);
} filter_map SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 102400);
    __type(key, struct sock *);
    __type(value, u64);
} checked_socks SEC(".maps");

void __always_inline set_addr_pair_by_sock(struct sock *sk, struct addr_pair *ap)
{
    bpf_probe_read(&ap->daddr, sizeof(ap->daddr), &sk->__sk_common.skc_daddr);
    bpf_probe_read(&ap->dport, sizeof(ap->dport), &sk->__sk_common.skc_dport);
    bpf_probe_read(&ap->saddr, sizeof(ap->saddr), &sk->__sk_common.skc_rcv_saddr);
    bpf_probe_read(&ap->sport, sizeof(ap->sport), &sk->__sk_common.skc_num);
    ap->dport = bpf_ntohs(ap->dport);
}

__always_inline int event_sock_filter(struct sock *sk)
{
    __u32 key = 0;
    __u16 protocol;
    struct filter *filter = NULL;
    struct addr_pair ap = {};
    u64 *ts;
    u64 filter_ts;
    u32 pid = bpf_get_current_pid_tgid();

    ts = bpf_map_lookup_elem(&checked_socks, &sk);
    filter = bpf_map_lookup_elem(&filter_map, &key);

    // no filter, just skip it.
    if (!filter)
        return -1;

    if (filter->pid && filter->pid != pid)
        return -1;

    filter_ts = filter->ts;
    // have checked, just skip it.
    if (ts && filter_ts == *ts)
        return -1;

    protocol = bpf_core_sock_sk_protocol(sk);
    if (filter->protocol && filter->protocol != protocol)
        return -1;

    set_addr_pair_by_sock(sk, &ap);
    if (filter->ap.daddr && ap.daddr != filter->ap.daddr)
        return -1;
    if (filter->ap.saddr && ap.saddr != filter->ap.saddr)
        return -1;
    if (filter->ap.dport && ap.dport != filter->ap.dport)
        return -1;
    if (filter->ap.sport && ap.sport != filter->ap.sport)
        return -1;

    bpf_map_update_elem(&checked_socks, &sk, &filter_ts, BPF_ANY);
    return 0;
}

__always_inline void fill_net_params(struct net *net, struct net_params *np)
{
    // BPF_CORE_READ_INTO(&np->insert_failed, net, ct.stat, insert_failed);
}

__always_inline void fill_tcp_params(struct sock *sk, struct tcp_params *tp)
{
    struct inet_connection_sock *icsk = (struct inet_connection_sock *)sk;
    struct tcp_sock *ts = (struct tcp_sock *)tp;
    
    bpf_probe_read(&tp->state, sizeof(tp->state), &sk->__sk_common.skc_state);
    
    // queue
    tp->sk_ack_backlog = bpf_core_sock_ack_backlog(sk);
    tp->icsk_accept_queue = bpf_core_reqsk_synqueue_len(sk);
    bpf_probe_read(&tp->sk_max_ack_backlog, sizeof(tp->sk_max_ack_backlog), &sk->sk_max_ack_backlog);

    // memory
    bpf_probe_read(&tp->sk_wmem_queued, sizeof(tp->sk_wmem_queued), &sk->sk_wmem_queued);
    bpf_probe_read(&tp->sndbuf, sizeof(tp->sndbuf), &sk->sk_sndbuf);
    bpf_probe_read(&tp->rmem_alloc, sizeof(tp->rmem_alloc), &sk->sk_backlog.rmem_alloc.counter);
    bpf_probe_read(&tp->sk_rcvbuf, sizeof(tp->sk_rcvbuf), &sk->sk_rcvbuf);

    //packet
    bpf_probe_read(&tp->drop, sizeof(tp->drop), &sk->sk_drops.counter);
    bpf_probe_read(&tp->retran, sizeof(tp->retran), &ts->total_retrans);
    if (bpf_core_field_exists(ts->rcv_ooopack))
        bpf_probe_read(&tp->ooo, sizeof(tp->ooo), &ts->rcv_ooopack);
}

#define offsetof(TYPE, MEMBER) ((int)&((TYPE *)0)->MEMBER)
#define container_of(ptr, type, member) ({          \
    const typeof(((type *)0)->member) * __mptr = (ptr); \
    (type *)((char *)__mptr - offsetof(type, member)); })

SEC("kprobe/sockfs_dname")
int BPF_KPROBE(sockfs_dname, struct dentry *dentry, char *buffer, int buflen)
{

    struct inode *inode = NULL;

    bpf_probe_read(&inode, sizeof(inode), &dentry->d_inode);

    if (!inode)
        return 0;

    struct socket_alloc *sa = container_of(inode, struct socket_alloc, vfs_inode);
    struct socket *socket = &sa->socket;
    struct sock *sk;

    bpf_probe_read(&sk, sizeof(sk), &socket->sk);
    if (!sk)
        return 0;

    if (event_sock_filter(sk))
        return 0;

    struct event event = {};
    // bpf_probe_read(&net, sizeof(net), &sk->__sk_common.skc_net);

    bpf_probe_read(&event.i_ino, sizeof(event.i_ino), &inode->i_ino);
    event.protocol = bpf_core_sock_sk_protocol(sk);
    bpf_probe_read(&event.ap.daddr, sizeof(event.ap.daddr), &sk->__sk_common.skc_daddr);
    bpf_probe_read(&event.ap.dport, sizeof(event.ap.dport), &sk->__sk_common.skc_dport);
    bpf_probe_read(&event.ap.saddr, sizeof(event.ap.saddr), &sk->__sk_common.skc_rcv_saddr);
    bpf_probe_read(&event.ap.sport, sizeof(event.ap.sport), &sk->__sk_common.skc_num);
    event.ap.dport = bpf_ntohs(event.ap.dport);

    // net
    // if (net)
    // {
    //     event.has_net = 1;
    //     fill_net_params(net, &event.np);
    // }

    fill_tcp_params(sk, &event.tp);

    bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU, &event, sizeof(event));
    return 0;
}

char _license[] SEC("license") = "GPL";