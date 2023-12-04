#define BPF_NO_GLOBAL_DATA
#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_endian.h>

#include "tcpping.h"

// copy fron pingtrace.h
struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __type(key, int);
    __type(value, struct irq);
    __uint(max_entries, 1024);
} irq_events SEC(".maps");

SEC("kprobe/skb_recv_done")
int BPF_KPROBE(kprobe__skb_recv_done)
{
    int cpu = bpf_get_smp_processor_id();
    struct irq *hi = bpf_map_lookup_elem(&irq_events, &cpu);
    if (!hi)
        return 0;

    unsigned long long cnt = hi->cnt & (IRQ_RING_SIZE - 1);
    hi->cnt++;
    hi->tss[cnt] = bpf_ktime_get_ns();
    return 0;
}

SEC("kprobe/mlx5e_completion_event")
int BPF_KPROBE(kprobe__mlx5e_completion_event)
{
    int cpu = bpf_get_smp_processor_id();
    struct irq *hi = bpf_map_lookup_elem(&irq_events, &cpu);
    if (!hi)
        return 0;

    unsigned long long cnt = hi->cnt & (IRQ_RING_SIZE - 1);
    hi->cnt++;
    hi->tss[cnt] = bpf_ktime_get_ns();
    return 0;
}

struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __type(key, int);
    __type(value, struct softirq);
    __uint(max_entries, 1024);
} softirq_events SEC(".maps");

SEC("kprobe/__do_softirq")
int BPF_KPROBE(kprobe____do_softirq)
{
    int cpu = bpf_get_smp_processor_id();
    struct softirq *si = bpf_map_lookup_elem(&softirq_events, &cpu);
    if (!si)
        return 0;

    unsigned long long cnt = si->cnt & (SOFTIRQ_RING_SIZE - 1);
    si->cnt++;
    si->tss[cnt] = bpf_ktime_get_ns();
    return 0;
}

struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __type(key, int);
    __type(value, struct wakeup);
    __uint(max_entries, 1024);
} wakeup_events SEC(".maps");

// int wake_up_process(struct task_struct *p)
SEC("kprobe/wake_up_process")
int BPF_KPROBE(kprobe__wake_up_process, struct task_struct *p)
{
    unsigned char comm[16];
    bpf_probe_read(comm, sizeof(comm), &p->comm);

    if (comm[0] != 'k' || comm[1] != 's' || comm[2] != 'o' || comm[3] != 'f' || comm[4] != 't' || comm[5] != 'i' || comm[6] != 'r')
        return 0;

    int cpu = bpf_get_smp_processor_id();
    struct wakeup *wu = bpf_map_lookup_elem(&wakeup_events, &cpu);
    if (!wu)
        return 0;

    unsigned long long cnt = wu->cnt & (WAKEUP_RING_SIZE - 1);
    wu->cnt++;
    wu->tss[cnt] = bpf_ktime_get_ns();
    return 0;
}

struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __type(key, int);
    __type(value, struct sched);
    __uint(max_entries, 1024);
} sched_events SEC(".maps");

struct tp_sched_switch_arg
{
    u64 pad;
    char prev_comm[16];
    pid_t prev_pid;
    int prev_prio;
    long prev_state;
    char next_comm[16];
    pid_t next_pid;
};

SEC("tracepoint/sched/sched_switch")
int tp_sched_switch(struct tp_sched_switch_arg *arg)
{
    int cpu = bpf_get_smp_processor_id();
    struct sched *ring = NULL;

    ring = bpf_map_lookup_elem(&sched_events, &cpu);
    if (!ring)
        return 0;

    u64 cnt = ring->cnt & (SCHEDSWITCH_RING_SIZE - 1);
    ring->cnt++;

    ring->ss[cnt].ts = bpf_ktime_get_ns();
    ring->ss[cnt].next_pid = arg->next_pid;
    ring->ss[cnt].prev_pid = arg->prev_pid;

    __builtin_memcpy(ring->ss[cnt].next_comm, arg->next_comm, 16);
    __builtin_memcpy(ring->ss[cnt].prev_comm, arg->prev_comm, 16);

    return 0;
}

static __always_inline void save_all_events(struct tcpping *tp, int cpu)
{
    struct irq *hi = bpf_map_lookup_elem(&irq_events, &cpu);
    if (hi)
        tp->irq = *hi;

    struct softirq *si = bpf_map_lookup_elem(&softirq_events, &cpu);
    if (si)
        tp->sirq = *si;

    struct wakeup *wu = bpf_map_lookup_elem(&wakeup_events, &cpu);
    if (wu)
        tp->wu = *wu;

    struct sched *ss = bpf_map_lookup_elem(&sched_events, &cpu);
    if (ss)
        tp->sched = *ss;
}

struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, u32);
    __type(value, struct filter);
} filters SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, u32);
    __type(value, struct tcpping);
} latency SEC(".maps");

struct msghdr___310
{
    struct iovec *msg_iov;
};

__always_inline void get_icmphdr_with_l4(struct sk_buff *skb, struct tcphdr *th)
{
    char *head;
    u16 transport_header;

    bpf_probe_read(&transport_header, sizeof(transport_header), &skb->transport_header);
    bpf_probe_read(&head, sizeof(head), &skb->head);
    bpf_probe_read(th, sizeof(*th), head + transport_header);
}

__always_inline bool get_tcphdr_with_l3(struct sk_buff *skb, struct tcphdr *th)
{
    struct iphdr ih = {0};
    u16 network_header;
    char *head;
    bpf_probe_read(&network_header, sizeof(network_header), &skb->network_header);
    bpf_probe_read(&head, sizeof(head), &skb->head);
    bpf_probe_read(&ih, sizeof(ih), head + network_header);

    if (ih.protocol == IPPROTO_TCP)
    {
        bpf_probe_read(th, sizeof(*th), head + network_header + (ih.ihl << 2));
        return true;
    }
    return false;
}

SEC("kprobe/raw_sendmsg")
int BPF_KPROBE(kprobe__raw_sendmsg, u64 arg1, u64 arg2, u64 arg3)
{
    struct sock *sk = NULL;
    struct msghdr___310 msg310 = {};
    int key = 0;

    int pid = bpf_get_current_pid_tgid() >> 32;
    struct filter *filter = bpf_map_lookup_elem(&filters, &key);
    if (!filter || filter->pid != pid)
        return 0;

    if (!bpf_core_field_exists(msg310.msg_iov)) // alinux2 & 3
        sk = (struct sk_buff *)arg1;
    else // centos 310
        sk = (struct sk_buff *)arg2;

    filter->sock = (u64)sk;
    struct tcpping *tp = bpf_map_lookup_elem(&latency, &key);
    if (!tp)
        return 0;

    tp->stages[TCPPING_TX_ENTRY].ts = bpf_ktime_get_ns();
    return 0;
}

struct tp_net_arg
{
    u32 pad[2];
    struct sk_buff *skbaddr;
};


SEC("tracepoint/net/net_dev_xmit")
int tp_net_dev_xmit(struct tp_net_arg *args)
{
    struct sk_buff *skb = args->skbaddr;
    struct sock *sk;
    int key = 0;
    struct filter *filter = bpf_map_lookup_elem(&filters, &key);
    if (!filter)
        return 0;

    bpf_probe_read(&sk, sizeof(sk), &skb->sk);
    if (filter->sock != sk)
        return 0;

    struct tcpping *tp = bpf_map_lookup_elem(&latency, &key);
    if (!tp)
        return 0;
    tp->stages[TCPPING_TX_EXIT].ts = bpf_ktime_get_ns();
    return 0;
}

unsigned long long load_byte(void *skb,
                             unsigned long long off) asm("llvm.bpf.load.byte");
unsigned long long load_half(void *skb,
                             unsigned long long off) asm("llvm.bpf.load.half");
unsigned long long load_word(void *skb,
                             unsigned long long off) asm("llvm.bpf.load.word");

SEC("socket")
int socket_tcp(struct __sk_buff *skb)
{
    __u64 nhoff = 0;
    __u64 ip_proto;
    __u64 verlen;
    u32 ports;
    u16 dport;
    int key = 0;

    ip_proto = load_byte(skb, nhoff + offsetof(struct iphdr, protocol));
    if (ip_proto != 6)
        return 0;

    verlen = load_byte(skb, nhoff + 0);
    if (verlen == 0x45)
        nhoff += 20;
    else
        nhoff += (verlen & 0xF) << 2;

    ports = load_word(skb, nhoff);
    dport = (u16)ports;
    struct filter *filter = bpf_map_lookup_elem(&filters, &key);
    if (!filter)
        return 0;
    
    if (dport == filter->lport)
    {
        int key = 0;
        struct tcpping *tp = bpf_map_lookup_elem(&latency, &key);
        if (!tp)
            return 0;
        tp->stages[TCPPING_RX_EXIT].ts = bpf_ktime_get_ns();
        return -1;
    }
    return 0;
}


SEC("tracepoint/net/netif_receive_skb")
int tp_netif_receive_skb(struct tp_net_arg *args)
{
    struct sk_buff *skb = args->skbaddr;
    struct tcphdr th = {0};
    int key = 0;
    if (!get_tcphdr_with_l3(skb, &th))
        return 0;

    struct filter *filter = bpf_map_lookup_elem(&filters, &key);
    if (!filter)
        return 0;

    if (filter->be_lport != th.dest || filter->be_rport != th.source)
        return 0;

    int cpu = bpf_get_smp_processor_id();
    struct tcpping *tp = bpf_map_lookup_elem(&latency, &key);
    if (!tp)
        return 0;

    tp->stages[TCPPING_RX_ENTRY].ts = bpf_ktime_get_ns();
    save_all_events(tp, cpu);
    return 0;
}

char _license[] SEC("license") = "GPL";