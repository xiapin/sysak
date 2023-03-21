/*
 * Author: Chen Tao
 * Create: Mon Nov 14 11:58:32 2022
 */
#include "vmlinux/common.h"
#include "net.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include "nginx.h"

#define _(P)                                        \
    ({                                              \
        typeof(P) val;                              \
        bpf_probe_read_user((unsigned char *)&val, sizeof(val), (const void *)&P); \
        val;                                        \
    }) 

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, struct ip_addr);
    __type(value, struct ngx_trace);
    __uint(max_entries, 10240);
} ngx_upstream_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, struct ip_addr); //cpu
    __type(value, struct ngx_trace);
    __uint(max_entries, 256);
} ngx_request_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, u32); //cpu
    __type(value, struct ngx_worker);
    __uint(max_entries, 256);
} worker_exit_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
    __type(key, int);
    __type(value, int);
} events_map SEC(".maps");

#ifndef NULL
#define NULL (void *)0
#endif

static void copy_ip_addr(const struct sockaddr *addr, struct ip_addr *ip)
{
    ip->family = _(addr->sa_family);
    if (ip->family == AF_INET) {
        struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
        ip->ipaddr.ip4 = _(addr_in->sin_addr.s_addr);
        ip->port = _(addr_in->sin_port);
    } else {
        struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)addr;
        ip->port = _(addr_in6->sin6_port);
        bpf_probe_read_user(ip->ipaddr.ip6, IP6_LEN, addr_in6->sin6_addr.in6_u.u6_addr8);
    }
    return;
}

SEC("uprobe/ngx_close_connection")
int ngx_close_connection_fn(struct pt_regs *ctx) 
{
    struct ngx_connection_s *conn = (struct ngx_connection_s *)PT_REGS_PARM1(ctx);
    struct ngx_trace *trace;

    struct ip_addr src_ip = {};
    struct sockaddr *client_addr;

    // cilent ip
    client_addr = _(conn->sockaddr);
    copy_ip_addr(client_addr, &src_ip);
    trace = (struct ngx_trace *)bpf_map_lookup_elem(&ngx_request_map, &src_ip);
    if (trace == NULL)
        return 0;
    bpf_map_delete_elem(&ngx_request_map, &src_ip);
    return 0;
}

SEC("uprobe/ngx_http_create_request")
int ngx_http_create_request_fn(struct pt_regs *ctx)
{
    struct ngx_connection_s *con = (struct ngx_connection_s *)PT_REGS_PARM1(ctx);
    int pid = bpf_get_current_pid_tgid() >> 32;
    int cpu = bpf_get_smp_processor_id(); 
    struct ngx_trace *ntp;
    struct ngx_trace trace = {};
    struct sockaddr *addr = _(con->sockaddr);
    struct ngx_worker wk = {};

    copy_ip_addr(addr, &trace.srcip);
    addr = _(con->local_sockaddr);
    copy_ip_addr(addr, &trace.ngxip);
    ntp = bpf_map_lookup_elem(&ngx_request_map, &trace.srcip);
    if (ntp == NULL) {
        struct ngx_worker wk = {
            .pid = pid,
            .cpu = cpu,
            .handle_cnt = 1,
        };
        trace.nw = wk;
        bpf_map_update_elem(&ngx_request_map, &trace.srcip, &trace, BPF_NOEXIST);
    } else {
        ntp->nw.handle_cnt++;
        trace.nw = ntp->nw;
        bpf_perf_event_output(ctx, &events_map, BPF_F_CURRENT_CPU, &trace, sizeof(trace));
    }

    return 0;
}

SEC("tracepoint/sched/sched_process_free")
int ngx_release_task_fn(struct pt_regs *ctx)
{
    char comm[16];
    u32 cpu;
    u32 pid = bpf_get_current_pid_tgid() >> 32;
    bpf_get_current_comm(comm, sizeof(comm));
    if (!(comm[0] == 'n' && comm[1] == 'g' && comm[2] == 'i' && comm[3] == 'n' &&
        comm[4] == 'x' && comm[5] == '\0'))
            return 0;

    cpu = bpf_get_smp_processor_id(); 
    struct ngx_worker wk = {
        .pid = pid,
        .cpu = cpu,
        .exit_cnt = 1,
    };
    //bpf_map_update_elem(&worker_exit_map, &cpu, &wk, BPF_NOEXIST);
    //bpf_perf_event_output(ctx, &events_map, BPF_F_CURRENT_CPU, &wk, sizeof(wk));
    return 0;
}

char LICENSE[] SEC("license") = "GPL";
