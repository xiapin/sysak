#ifndef __ICMP_H
#define __ICMP_H

#define ICMP_ECHO 8
#define ICMP_ECHOREPLY 0

#define IRQ_RING_SIZE 8
#define SOFTIRQ_RING_SIZE 8
#define WAKEUP_RING_SIZE 8
#define SCHEDSWITCH_RING_SIZE 8

struct ping_key
{
    unsigned short seq;
    unsigned short id;
};

struct ping_stage
{
    unsigned long long ts;
    unsigned int cpu;
};

enum PING_SENDER_STAGE
{
    PING_SND = 0,
    PING_DEV_QUEUE,
    PING_DEV_XMIT,
    PING_NETIF_RCV,
    PING_ICMP_RCV,
    PING_RCV,
    PING_MAX,
};

enum EVENT_TYPE
{
    PING,
    IRQ,
    SOFTIRQ,
    WAKEUP,
    SCHED,
};

struct ping_sender
{
    unsigned long long ty;
    struct ping_key key;
    struct ping_stage stages[PING_MAX];
};

struct irq
{
    unsigned long long ty;
    unsigned long long tss[IRQ_RING_SIZE];
    unsigned long long cnt;
};

struct softirq
{
    unsigned long long ty;
    unsigned long long tss[SOFTIRQ_RING_SIZE];
    unsigned long long cnt;
};

struct wakeup
{
    unsigned long long ty;
    unsigned long long tss[SOFTIRQ_RING_SIZE];
    unsigned long long cnt;
};

struct sched
{
    unsigned long long ty;
    struct
    {
        int prev_pid;
        int next_pid;
        unsigned char prev_comm[16];
        unsigned char next_comm[16];
        unsigned long long ts;
    } ss[SCHEDSWITCH_RING_SIZE];
    unsigned long long cnt;
};

#ifdef __VMLINUX_H__

struct
{
    __uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
    __uint(key_size, sizeof(u32));
    __uint(value_size, sizeof(u32));
} perf_events SEC(".maps");

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

static __always_inline void output_all_events(void *ctx, int cpu)
{
    struct irq *hi = bpf_map_lookup_elem(&irq_events, &cpu);
    if (hi)
    {
        hi->ty = IRQ;
        bpf_perf_event_output(ctx, &perf_events, BPF_F_CURRENT_CPU, hi, sizeof(*hi));
    }

    struct softirq *si = bpf_map_lookup_elem(&softirq_events, &cpu);
    if (si)
    {
        si->ty = SOFTIRQ;
        bpf_perf_event_output(ctx, &perf_events, BPF_F_CURRENT_CPU, si, sizeof(*si));
    }

    struct wakeup *wu = bpf_map_lookup_elem(&wakeup_events, &cpu);
    if (wu)
    {
        wu->ty = WAKEUP;
        bpf_perf_event_output(ctx, &perf_events, BPF_F_CURRENT_CPU, wu, sizeof(*wu));
    }

    struct sched *ss = bpf_map_lookup_elem(&sched_events, &cpu);
    if (ss)
    {
        ss->ty = SCHED;
        bpf_perf_event_output(ctx, &perf_events, BPF_F_CURRENT_CPU, ss, sizeof(*ss));
    }
}

#endif

#endif
