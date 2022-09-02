

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
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 102400);
    __type(key, struct sock *);
    __type(value, u64);
} inner_checked_socks SEC(".maps");

__always_inline int event_sock_filter(struct sock *sk)
{
    __u32 key = 0;
    __u16 protocol;
    struct filter *filter = NULL;
    struct addr_pair ap = {};
    u64 *ts;
    u64 filter_ts;
    u32 pid = bpf_get_current_pid_tgid();

    ts = bpf_map_lookup_elem(&inner_checked_socks, &sk);
    filter = bpf_map_lookup_elem(&filter_map, &key);

    // no filter, just skip it.
    if (!filter)
        return -1;

    if (filter->pid && filter->pid != pid)
        return -1;

    filter_ts = filter->threshold;
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

    bpf_map_update_elem(&inner_checked_socks, &sk, &filter_ts, BPF_ANY);
    return 0;
}

__always_inline void fill_tcp_params(struct sock *sk, struct event *event)
{
    struct inet_connection_sock *icsk = (struct inet_connection_sock *)sk;
    struct tcp_sock *tp = (struct tcp_sock *)sk;
    
    bpf_probe_read(&event->state, sizeof(event->state), &sk->__sk_common.skc_state);
    
    // queue
    event->abnormal.sk_ack_backlog = bpf_core_sock_ack_backlog(sk);
    event->abnormal.icsk_accept_queue = bpf_core_reqsk_synqueue_len(sk);
    bpf_probe_read(&event->abnormal.sk_max_ack_backlog, sizeof(event->abnormal.sk_max_ack_backlog), &sk->sk_max_ack_backlog);

    // memory
    bpf_probe_read(&event->abnormal.sk_wmem_queued, sizeof(event->abnormal.sk_wmem_queued), &sk->sk_wmem_queued);
    bpf_probe_read(&event->abnormal.sndbuf, sizeof(event->abnormal.sndbuf), &sk->sk_sndbuf);
    bpf_probe_read(&event->abnormal.rmem_alloc, sizeof(event->abnormal.rmem_alloc), &sk->sk_backlog.rmem_alloc.counter);
    bpf_probe_read(&event->abnormal.sk_rcvbuf, sizeof(event->abnormal.sk_rcvbuf), &sk->sk_rcvbuf);

    //packet
    bpf_probe_read(&event->abnormal.drop, sizeof(event->abnormal.drop), &sk->sk_drops.counter);
    bpf_probe_read(&event->abnormal.retran, sizeof(event->abnormal.retran), &tp->total_retrans);
    if (bpf_core_field_exists(tp->rcv_ooopack))
        bpf_probe_read(&event->abnormal.ooo, sizeof(event->abnormal.ooo), &tp->rcv_ooopack);
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
    bpf_probe_read(&event.abnormal.i_ino, sizeof(event.abnormal.i_ino), &inode->i_ino);
    event.protocol = bpf_core_sock_sk_protocol(sk);
    bpf_probe_read(&event.ap.daddr, sizeof(event.ap.daddr), &sk->__sk_common.skc_daddr);
    bpf_probe_read(&event.ap.dport, sizeof(event.ap.dport), &sk->__sk_common.skc_dport);
    bpf_probe_read(&event.ap.saddr, sizeof(event.ap.saddr), &sk->__sk_common.skc_rcv_saddr);
    bpf_probe_read(&event.ap.sport, sizeof(event.ap.sport), &sk->__sk_common.skc_num);
    event.ap.dport = bpf_ntohs(event.ap.dport);
    fill_tcp_params(sk, &event);

    bpf_perf_event_output(ctx, &perf_map, BPF_F_CURRENT_CPU, &event, sizeof(event));
    return 0;
}

char _license[] SEC("license") = "GPL";