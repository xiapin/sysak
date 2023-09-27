
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

static inline void debug_sock_role(struct sock_info *info)
{
        // __bpf_printk("role: %d\n", info->role);
}

static inline void debug_pid_info(struct pid_info *info)
{
        // __bpf_printk("container id: %s\n", info->container_id);
}

static inline void set_addr_pair_by_sock(struct sock *sk, struct addrpair *ap)
{
        bpf_probe_read(&ap->daddr, sizeof(ap->daddr), &sk->__sk_common.skc_daddr);
        bpf_probe_read(&ap->dport, sizeof(ap->dport), &sk->__sk_common.skc_dport);
        bpf_probe_read(&ap->saddr, sizeof(ap->saddr), &sk->__sk_common.skc_rcv_saddr);
        bpf_probe_read(&ap->sport, sizeof(ap->sport), &sk->__sk_common.skc_num);
        ap->dport = bpf_ntohs(ap->dport);
}

static inline void update_pid_info(struct pid_info *info)
{
        struct task_struct *curr_task;
        struct kernfs_node *knode, *pknode;

        info->valid = 1;
        bpf_get_current_comm(info->comm, sizeof(info->comm));
        curr_task = (struct task_struct *)bpf_get_current_task();
        knode = BPF_CORE_READ(curr_task, cgroups, subsys[0], cgroup, kn);
        pknode = BPF_CORE_READ(knode, parent);
        if (pknode != NULL)
        {
                char *aus;
                bpf_core_read(&aus, sizeof(void *), &knode->name);
                bpf_core_read_str(info->container_id, 128, aus);
#ifdef NTOPO_BPF_DEBUG
                debug_pid_info(info);
#endif
        }
        else
                info->container_id[0] = '\0';
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

static inline void update_nodes(struct sock_info *info, int role, int rt)
{
        struct node_info_key key = {0};
        key.addr = info->ap.saddr;

        struct node_info *ninfo = bpf_map_lookup_elem(&nodes, &key);
        if (!ninfo)
        {
                struct node_info val = {0};
                bpf_map_update_elem(&nodes, &key, &val, BPF_ANY);
        }

        ninfo = bpf_map_lookup_elem(&nodes, &key);
        if (!ninfo)
                return;

        ninfo->in_bytes += info->in_bytes;
        ninfo->out_bytes += info->out_bytes;
        info->in_bytes = 0;
        info->out_bytes = 0;
        ninfo->pid = info->pid;
        ninfo->requests++;
        if (role == ROLE_CLIENT)
        {
                ninfo->client_tot_rt_hz += 1;
                ninfo->client_tot_rt_us += rt;
                if (rt > ninfo->client_max_rt_us)
                {
                        ninfo->client_addr = info->ap.saddr;
                        ninfo->server_addr = info->ap.daddr;
                        ninfo->sport = info->ap.sport;
                        ninfo->dport = info->ap.dport;
                        ninfo->client_max_rt_us = rt;
                }
        }
        else
        {
                ninfo->server_tot_rt_hz += 1;
                ninfo->server_tot_rt_us += rt;
                if (rt > ninfo->server_max_rt_us)
                {
                        ninfo->server_addr = info->ap.saddr;
                        ninfo->client_addr = info->ap.daddr;
                        ninfo->sport = info->ap.dport;
                        ninfo->dport = info->ap.sport;
                        ninfo->server_max_rt_us = rt;
                }
        }

}

static inline bool try_add_sock(struct sock *sk)
{
        u64 tgid = bpf_get_current_pid_tgid();
        u32 pid = tgid >> 32;
        struct pid_info *pinfop;
        pinfop = bpf_map_lookup_elem(&pids, &pid);
        if (!pinfop)
                return false;
        if (!pinfop->valid)
                update_pid_info(pinfop);

        struct sock_info info = {0};
        info.pid = pid;
        set_addr_pair_by_sock(sk, &info.ap);
        if (info.ap.saddr == info.ap.daddr)
                return false;

        info.role = get_sock_role(&info, sk);
        // if (info.role == ROLE_SERVER) {
        //         int tmp;
        //         tmp = info.ap.saddr;
        //         info.ap.saddr = info.ap.daddr;
        //         info.ap.daddr = tmp;
        // }
        bpf_map_update_elem(&socks, &sk, &info, BPF_ANY);
#ifdef NTOPO_BPF_DEBUG
        debug_sock_role(&info);
#endif
        return true;
}

static inline void handle_client_send_request(struct sock_info *info, u64 ts)
{
        if (info->egress_min != 0 && info->ingress_min != 0)
        {
                u32 rt_us = (info->ingress_max - info->egress_min) / 1000;
                info->ingress_min = 0;
                info->egress_min = 0;
                update_nodes(info, ROLE_CLIENT, rt_us);
                update_edges(info, ROLE_CLIENT);
        }
}

static inline void handle_client_recv_response()
{
        // do nothing
}

static inline void handle_server_recv_request(struct sock_info *info, u64 ts)
{
        if (info->egress_min != 0 && info->ingress_min != 0)
        {
                u32 rt_us = (info->egress_max - info->ingress_min) / 1000;
                info->ingress_min = 0;
                info->egress_min = 0;
                update_nodes(info, ROLE_SERVER, rt_us);
                update_edges(info, ROLE_SERVER);
        }
}

static inline void handle_server_send_response(struct sock_info *info, u64 ts)
{
}

static inline void handle_client_close(struct sock_info *info, u64 ts)
{
        handle_client_send_request(info, ts);
}

static inline void handle_server_close(struct sock_info *info, u64 ts)
{
        handle_server_recv_request(info, ts);
}

// int tcp_sendmsg(struct sock *sk, struct msghdr *msg, size_t size)
SEC("kprobe/tcp_sendmsg_locked")
int BPF_KPROBE(kprobe_tcp_sendmsg_locked, struct sock *sk, struct msghdr *msg, size_t size)
{
        struct sock_info *info = bpf_map_lookup_elem(&socks, &sk);
        if (!info)
                if (try_add_sock(sk))
                        info = bpf_map_lookup_elem(&socks, &sk);
        if (!info)
                return 0;
        u64 ts = bpf_ktime_get_ns();
        enum role role = get_sock_role(info, sk);
        if (role == ROLE_CLIENT)
                handle_client_send_request(info, ts);
        else
                handle_server_send_response(info, ts);

        info->out_bytes += size;
        if (info->egress_min == 0)
                info->egress_min = ts;
        info->egress_max = ts;
        return 0;
}

#if 0
struct tcp_rcv_space_adjust_args
{
        u32 pad[2];
        struct sock *sk;
};

SEC("tracepoint/tcp/tcp_rcv_space_adjust")
int tracepoint_tcp_rcv_space_adjust(struct tcp_rcv_space_adjust_args *ctx)
{
        struct sock *sk = ctx->sk;
        struct sock_info *info = bpf_map_lookup_elem(&socks, &sk);
        if (!info)
                if (try_add_sock(sk))
                        info = bpf_map_lookup_elem(&socks, &sk);
        if (!info)
                return 0;

        u64 ts = bpf_ktime_get_ns();
        enum role role = get_sock_role(info, sk);
        if (role == ROLE_CLIENT)
                handle_client_recv_response(info, ts);
        else
                handle_server_recv_request(info, ts);

        if (info->ingress_min == 0)
                info->ingress_min = ts;
        info->ingress_max = ts;
        return 0;
}
#endif

SEC("kprobe/tcp_cleanup_rbuf")
int BPF_KPROBE(kprobe_tcp_cleanup_rbuf, struct sock *sk, int copied)
{
        struct sock_info *info = bpf_map_lookup_elem(&socks, &sk);
        if (!info)
                if (try_add_sock(sk))
                        info = bpf_map_lookup_elem(&socks, &sk);
        if (!info)
                return 0;

        u64 ts = bpf_ktime_get_ns();
        enum role role = get_sock_role(info, sk);
        if (role == ROLE_CLIENT)
                handle_client_recv_response(info, ts);
        else
                handle_server_recv_request(info, ts);

        info->in_bytes += copied;
        if (info->ingress_min == 0)
                info->ingress_min = ts;
        info->ingress_max = ts;
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
        if (role == ROLE_CLIENT)
                handle_client_close(info, ts);
        else
                handle_server_close(info, ts);

        bpf_map_delete_elem(&socks, &sk);
        return 0;
}

char _license[] SEC("license") = "GPL";