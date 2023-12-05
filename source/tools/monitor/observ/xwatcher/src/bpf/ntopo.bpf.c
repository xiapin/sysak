
#define NTOPO_BPF_DEBUG
#define BPF_NO_GLOBAL_DATA

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_endian.h>
#include "ntopo.h"

struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, struct sock *);
    __type(value, struct sock_info);
    __uint(max_entries, 1024000);
} socks SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, u32);
    __type(value, struct pid_info);
    __uint(max_entries, 1024);
} pids SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, struct edge_info_key);
    __type(value, struct edge_info);
    __uint(max_entries, 1024);
} edges SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, struct node_info_key);
    __type(value, struct node_info);
    __uint(max_entries, 1024);
} nodes SEC(".maps");

static inline void set_addr_pair_by_sock(struct sock *sk, struct addrpair *ap)
{
    bpf_probe_read(&ap->daddr, sizeof(ap->daddr), &sk->__sk_common.skc_daddr);
    bpf_probe_read(&ap->dport, sizeof(ap->dport), &sk->__sk_common.skc_dport);
    bpf_probe_read(&ap->saddr, sizeof(ap->saddr), &sk->__sk_common.skc_rcv_saddr);
    bpf_probe_read(&ap->sport, sizeof(ap->sport), &sk->__sk_common.skc_num);
    ap->dport = bpf_ntohs(ap->dport);
}

static inline void update_pid_info(struct pid_info *info, int in_bytes, int out_bytes)
{
    struct task_struct *curr_task;
    struct kernfs_node *knode, *pknode;

    info->in_bytes += in_bytes;
    info->out_bytes += out_bytes;
}

static inline enum role get_sock_role(struct sock_info *info, struct sock *sk)
{
    if (info->role == ROLE_UNKNOWN)
    {
        int max_ack_backlog = 0;
        bpf_probe_read(&max_ack_backlog, sizeof(max_ack_backlog), &sk->sk_max_ack_backlog);

        info->role = max_ack_backlog == 0 ? ROLE_CLIENT : ROLE_SERVER;
    }
    return info->role;
}

static inline void update_edges(struct sock_info *info, int role)
{
    struct edge_info_key key = {0};
    struct edge_info val = {0};

    key.saddr = info->ap.saddr;
    key.daddr = info->ap.daddr;
    if (role == ROLE_SERVER)
    {
        int tmp = key.saddr;
        key.saddr = key.daddr;
        key.daddr = tmp;
    }
    bpf_map_update_elem(&edges, &key, &val, BPF_ANY);
}

static inline void update_nodes(struct sock_info *info)
{
    struct node_info_key key = {0};
    key.addr = info->ap.saddr;

    struct node_info *ninfo = bpf_map_lookup_elem(&nodes, &key);
    if (!ninfo)
    {
        struct node_info val = {0};
        bpf_map_update_elem(&nodes, &key, &val, BPF_ANY);
    }
}


// int tcp_sendmsg(struct sock *sk, struct msghdr *msg, size_t size)
SEC("kprobe/tcp_sendmsg_locked")
int BPF_KPROBE(kprobe_tcp_sendmsg_locked, struct sock *sk, struct msghdr *msg, size_t size)
{
    if (size == 0)
        return 0;
    
    u64 tgid = bpf_get_current_pid_tgid();
    u32 pid = tgid >> 32;
    struct pid_info *pinfop;
    pinfop = bpf_map_lookup_elem(&pids, &pid);
    if (!pinfop)
        return 0;

    update_pid_info(pinfop, 0, size);
    
    struct sock_info *infop = bpf_map_lookup_elem(&socks, &sk);
    if (!infop)
    {
            struct sock_info info = {0};
            info.pid = pid;
            set_addr_pair_by_sock(sk, &info.ap);
            if (info.ap.saddr == info.ap.daddr)
                return 0;

            info.role = get_sock_role(&info, sk);
            bpf_map_update_elem(&socks, &sk, &info, BPF_ANY);
    }
    infop = bpf_map_lookup_elem(&socks, &sk);
    if (!infop)
        return 0;
    
    enum role role = get_sock_role(infop, sk);
    
    update_nodes(infop);
    update_edges(infop, role);
    return 0;
}

struct tcp_rcv_space_adjust_args
{
    u32 pad[2];
    struct sock *sk;
};

SEC("tracepoint/tcp/tcp_rcv_space_adjust")
int tracepoint_tcp_rcv_space_adjust(struct tcp_rcv_space_adjust_args *ctx)
{
    struct sock *sk = ctx->sk;
    u64 tgid = bpf_get_current_pid_tgid();
    u32 pid = tgid >> 32;
    struct pid_info *pinfop;
    pinfop = bpf_map_lookup_elem(&pids, &pid);
    if (!pinfop)
        return 0;
    
    struct tcp_sock *tp = sk;
    u32 copied_seq, seq, in_bytes;
    bpf_probe_read(&copied_seq, sizeof(copied_seq), &tp->copied_seq);
    bpf_probe_read(&seq, sizeof(copied_seq), &tp->rcvq_space.seq);
    in_bytes = copied_seq - seq;
    update_pid_info(pinfop, in_bytes, 0);

    struct sock_info *infop = bpf_map_lookup_elem(&socks, &sk);
    if (!infop)
    {
            struct sock_info info = {0};
            info.pid = pid;
            set_addr_pair_by_sock(sk, &info.ap);
            if (info.ap.saddr == info.ap.daddr)
                return 0;

            info.role = get_sock_role(&info, sk);
            bpf_map_update_elem(&socks, &sk, &info, BPF_ANY);
    }
    infop = bpf_map_lookup_elem(&socks, &sk);
    if (!infop)
        return 0;

    enum role role = get_sock_role(infop, sk);
    update_nodes(infop);
    update_edges(infop, role);
    return 0;
}

// void tcp_close(struct sock *sk, long timeout);
SEC("kprobe/tcp_close")
int BPF_KPROBE(kprobe_tcp_close, struct sock *sk)
{
    struct sock_info *info = bpf_map_lookup_elem(&socks, &sk);
    if (!info)
        return 0;

    u64 ts = bpf_ktime_get_ns();
    enum role role = get_sock_role(info, sk);
    bpf_map_delete_elem(&socks, &sk);
    return 0;
}

char _license[] SEC("license") = "GPL";