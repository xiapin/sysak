//
// Created by 廖肇燕 on 2023/2/24.
//

#include <vmlinux.h>
#include <coolbpf.h>
#include "net_retrans.h"

struct liphdr {
    __u8 ver_hdl;
    __u8 tos;
    __be16 tot_len;
    __be16 id;
    __be16 frag_off;
    __u8 ttl;
    __u8 protocol;
    __sum16 check;
    __be32 saddr;
    __be32 daddr;
};

#define MAX_ENTRY 128
#define BPF_F_FAST_STACK_CMP	(1ULL << 9)
#define KERN_STACKID_FLAGS	(0 | BPF_F_FAST_STACK_CMP)
#define _(P) ({typeof(P) val = 0; bpf_probe_read(&val, sizeof(val), &P); val;})

BPF_PERF_OUTPUT(perf, 1024);
BPF_STACK_TRACE(stack, MAX_ENTRY);
BPF_ARRAY(outCnt, u64, NET_RETRANS_TYPE_MAX + 1);

static inline void addCnt(int k, u64 val) {
    k += 1;
    u64 *pv = bpf_map_lookup_elem(&outCnt, &k);
    if (pv) {
        __sync_fetch_and_add(pv, val);
    } else {
        bpf_map_update_elem(&outCnt, &k, &val, BPF_ANY);
    }
}

static inline int get_tcp_info(struct data_t* pdata, struct tcp_sock *ts)
{
    pdata->rcv_nxt = BPF_CORE_READ(ts, rcv_nxt);
    pdata->rcv_wup = BPF_CORE_READ(ts, rcv_wup);
    pdata->snd_nxt = BPF_CORE_READ(ts, snd_nxt);
    pdata->snd_una = BPF_CORE_READ(ts, snd_una);
    pdata->copied_seq = BPF_CORE_READ(ts, copied_seq);
    pdata->snd_wnd = BPF_CORE_READ(ts, snd_wnd);
    pdata->rcv_wnd = BPF_CORE_READ(ts, rcv_wnd);

    pdata->lost_out = BPF_CORE_READ(ts, lost_out);
    pdata->packets_out = BPF_CORE_READ(ts, packets_out);
    pdata->retrans_out = BPF_CORE_READ(ts, retrans_out);
    pdata->sacked_out = BPF_CORE_READ(ts, sacked_out);
    pdata->reordering = BPF_CORE_READ(ts, reordering);
    return 0;
}

static inline int get_skb_info(struct data_t* pdata, struct sk_buff *skb, u32 type)
{
    u16 offset;
    u8 ihl;
    void* head;
    struct liphdr *piph;
    struct tcphdr *ptcph;

    pdata->type = type;
    pdata->sk_state = 0;

    head = (void*)BPF_CORE_READ(skb, head);
    offset = BPF_CORE_READ(skb, network_header);
    piph = (struct liphdr *)(head + offset);
    ihl = _(piph->ver_hdl) & 0x0f;
    ptcph = (struct tcphdr *)((void *)piph + ihl * 4);

    pdata->ip_dst = _(piph->daddr);
    pdata->dport = BPF_CORE_READ(ptcph, dest);
    pdata->ip_src = _(piph->saddr);
    pdata->sport = BPF_CORE_READ(ptcph, source);
    return 0;
}

static inline void get_list_task(struct list_head* phead, struct data_t* e) {
    struct list_head *next = BPF_CORE_READ(phead, next);
    if (next) {
        wait_queue_entry_t *entry = container_of(next, wait_queue_entry_t, entry);
        struct poll_wqueues *pwq = (struct poll_wqueues *)BPF_CORE_READ(entry, private);
        if (pwq)
        {
            struct task_struct* tsk = (struct task_struct*)BPF_CORE_READ(pwq, polling_task);
            if (tsk) {
                e->pid = BPF_CORE_READ(tsk, pid);
                bpf_probe_read(&e->comm[0], TASK_COMM_LEN, &tsk->comm[0]);
            }
        }
    }
}

static inline void get_sock_task(struct sock *sk, struct data_t* e) {
    struct socket_wq *wq = BPF_CORE_READ(sk, sk_wq);
    if (wq) {
        struct list_head* phead = (struct list_head*)((char *)wq + offsetof(struct socket_wq, wait.head));
        get_list_task(phead, e);
    }
}

static inline void get_task(struct data_t* pdata, struct sock *sk) {
    pdata->pid = 0;
    pdata->comm[0] = '\0';

    get_sock_task(sk, pdata);
}

static inline int get_info(struct data_t* pdata, struct sock *sk, u32 type)
{
    struct inet_sock *inet = (struct inet_sock *)sk;

    pdata->type = type;
    pdata->ip_dst = BPF_CORE_READ(sk, __sk_common.skc_daddr);
    pdata->dport = BPF_CORE_READ(sk, __sk_common.skc_dport);
    pdata->ip_src = BPF_CORE_READ(sk, __sk_common.skc_rcv_saddr);
    pdata->sport = BPF_CORE_READ(inet, inet_sport);
    pdata->sk_state = BPF_CORE_READ(sk, __sk_common.skc_state);
    return 0;
}

static inline int check_inner(unsigned int ip)
{
    int i;
    const unsigned int array[3][2] = {
            {0x0000000A, 0x000000ff},
            {0x000010AC, 0x0000f0ff},
            {0x0000A8C0, 0x0000ffff},
    };

    if (ip == 0) {
        return 1;
    }
#pragma unroll 3
    for (i =0; i < 3; i ++) {
        if ((ip & array[i][1]) == array[i][0]) {
            return 1;
        }
    }
    return 0;
}

static inline int check_ip(struct data_t* pdata) {
    return check_inner(pdata->ip_src) && check_inner(pdata->ip_dst);
}

SEC("kprobe/tcp_enter_loss")
int j_tcp_enter_loss(struct pt_regs *ctx)
{
    struct sock *sk;
    struct data_t data = {};
    u32 stat;

    sk = (struct sock *)PT_REGS_PARM1(ctx);
    stat = BPF_CORE_READ(sk, __sk_common.skc_state);
    if (stat != 1) {
        return 0;
    }
    get_task(&data, sk);
    addCnt(NET_RETRANS_TYPE_RTO, 1);
    get_info(&data, sk, NET_RETRANS_TYPE_RTO);
    data.stack_id = 0;
    get_tcp_info(&data, (struct tcp_sock *)sk);
    if (check_ip(&data)) {
        bpf_perf_event_output(ctx, &perf, BPF_F_CURRENT_CPU, &data, sizeof(data));
    }
    return 0;
}

SEC("kprobe/tcp_send_probe0")
int j_tcp_send_probe0(struct pt_regs *ctx)
{
    struct sock *sk;
    struct data_t data = {};
    u32 stat;

    sk = (struct sock *)PT_REGS_PARM1(ctx);
    stat = BPF_CORE_READ(sk, __sk_common.skc_state);
    if (stat == 0) {
        return 0;
    }

    addCnt(NET_RETRANS_TYPE_ZERO, 1);
    get_info(&data, sk, NET_RETRANS_TYPE_ZERO);
    data.stack_id = 0;
    get_task(&data, sk);
    get_tcp_info(&data, (struct tcp_sock *)sk);

    bpf_perf_event_output(ctx, &perf, BPF_F_CURRENT_CPU, &data, sizeof(data));
    return 0;
}

SEC("kprobe/tcp_v4_send_reset")
int j_tcp_v4_send_reset(struct pt_regs *ctx)
{
    struct sock *sk;
    struct data_t data = {};

    sk = (struct sock *)PT_REGS_PARM1(ctx);
    if (sk == NULL) {
        struct sk_buff *skb = (struct sk_buff *)PT_REGS_PARM2(ctx);
        addCnt(NET_RETRANS_TYPE_RST, 1);
        get_skb_info(&data, skb, NET_RETRANS_TYPE_RST);
        get_task(&data, NULL);
        data.stack_id = 0;
    }
    else {
        addCnt(NET_RETRANS_TYPE_RST_SK, 1);
        get_info(&data, sk, NET_RETRANS_TYPE_RST_SK);
        get_task(&data, sk);
        if (data.sk_state == 10) { // for listen cath skb info.
            struct sk_buff *skb = (struct sk_buff *)PT_REGS_PARM2(ctx);
            get_skb_info(&data, skb, NET_RETRANS_TYPE_RST_SK);
        }
        data.stack_id = bpf_get_stackid(ctx, &stack, KERN_STACKID_FLAGS);
    }
    if (check_ip(&data)) {
        bpf_perf_event_output(ctx, &perf, BPF_F_CURRENT_CPU, &data, sizeof(data));
    }
    return 0;
}

SEC("kprobe/tcp_send_active_reset")
int j_tcp_send_active_reset(struct pt_regs *ctx)
{
    struct sock *sk;
    struct data_t data = {};

    addCnt(NET_RETRANS_TYPE_RST_ACTIVE, 1);

    sk = (struct sock *)PT_REGS_PARM1(ctx);
    get_info(&data, sk, NET_RETRANS_TYPE_RST_ACTIVE);
    data.stack_id = bpf_get_stackid(ctx, &stack, KERN_STACKID_FLAGS);

    get_task(&data, sk);
    if (check_ip(&data)) {
        bpf_perf_event_output(ctx, &perf, BPF_F_CURRENT_CPU, &data, sizeof(data));
    }
    return 0;
}

#define TCP_SYN_SENT 2
#define TCPF_SYN_SENT (1 << TCP_SYN_SENT)
SEC("kprobe/tcp_retransmit_skb")
int j_tcp_retransmit_skb(struct pt_regs *ctx){
    struct sock *sk;
    unsigned char stat;

    sk = (struct sock *)PT_REGS_PARM1(ctx);

    stat = BPF_CORE_READ(sk, __sk_common.skc_state);
    if (stat == TCP_SYN_SENT)
    {
        struct data_t data = {};

        addCnt(NET_RETRANS_TYPE_SYN, 1);
        get_info(&data, sk, NET_RETRANS_TYPE_SYN);
        get_task(&data, sk);
        get_tcp_info(&data, (struct tcp_sock *)sk);
        bpf_perf_event_output(ctx, &perf, BPF_F_CURRENT_CPU, &data, sizeof(data));
    }
    return 0;
}

SEC("kprobe/tcp_rtx_synack")
int j_tcp_rtx_synack(struct pt_regs *ctx)
{
    struct sock *sk, *sk2;
    struct request_sock *req = (struct request_sock *)PT_REGS_PARM2(ctx);
    struct data_t data = {};

    addCnt(NET_RETRANS_TYPE_SYN_ACK, 1);
    sk = (struct sock *)PT_REGS_PARM1(ctx);
    sk2 = BPF_CORE_READ(req, sk);
    get_info(&data, sk2, NET_RETRANS_TYPE_SYN_ACK);
    get_task(&data, sk);
    get_tcp_info(&data, (struct tcp_sock *)sk2);
    bpf_perf_event_output(ctx, &perf, BPF_F_CURRENT_CPU, &data, sizeof(data));
    return 0;
}
