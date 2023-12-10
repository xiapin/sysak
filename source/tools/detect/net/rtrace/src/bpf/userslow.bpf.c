#define BPF_NO_GLOBAL_DATA
#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_endian.h>

#include "userslow.h"

struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 10240);
    __type(key, u64);
    __type(value, u64);
} sock_cookies SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1024);
    __type(key, u32);
    __type(value, struct sched_event);
} scheds SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
    __uint(key_size, sizeof(u32));
    __uint(value_size, sizeof(u32));
} perf_events SEC(".maps");

struct tcp_probe_arg
{
    u64 pad;
    u8 saddr[28];
    u8 daddr[28];
    u16 sport;
    u16 dport;
    u32 mark;
    u16 data_len;
    u32 snd_nxt;
    u32 snd_una;
    u32 snd_cwnd;
    u32 sshtresh;
    u32 snd_wnd;
    u32 srtt;
    u32 rcv_wnd;
    u64 sock_cookie;
};

SEC("tracepoint/tcp/tcp_probe")
int tp_tcp_probe(struct tcp_probe_arg *arg)
{
    u64 cookie = arg->sock_cookie;
    u64 ts = bpf_ktime_get_ns();
    bpf_map_update_elem(&sock_cookies, &cookie, &ts, BPF_ANY);
    return 0;
}

struct tcp_rcv_space_adjust_arg
{
    u64 pad;
    void *skaddr;
    u16 sport;
    u16 dport;
    u32 saddr;
    u32 daddr;
    u64 saddr_v6[2];
    u64 daddr_v6[2];
    u64 sock_cookie;
};

SEC("tracepoint/tcp/tcp_rcv_space_adjust")
int tp_tcp_rcv_space_adjust(struct tcp_rcv_space_adjust_arg *arg)
{
    u64 cookie = arg->sock_cookie;
    u64 *prev_ts = bpf_map_lookup_elem(&sock_cookies, &cookie);

    if (!prev_ts)
        return 0;

    u64 ts = bpf_ktime_get_ns();
    u64 delta = ts - *prev_ts;

    int key = 0;

    struct filter *filter = get_filter();
    if (filter && delta > filter->threshold)
    {
        struct slow_event event = {0};

        event.sport = arg->sport;
        event.dport = arg->dport;
        event.saddr = arg->saddr;
        event.daddr = arg->daddr;
        event.krcv_ts = *prev_ts;
        event.urcv_ts = ts;
        int cpu = bpf_get_smp_processor_id();
        struct sched_event *sched = bpf_map_lookup_elem(&scheds, &cpu);
        if (sched)
            event.sched = *sched;

        int tid = bpf_get_current_pid_tgid();
        struct thread_event *thread = bpf_map_lookup_elem(&threads, &tid);
        if (thread)
            event.thread = *thread;

        bpf_perf_event_output(arg, &perf_events, BPF_F_CURRENT_CPU, &event, sizeof(event));
    }

    return 0;
}

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
    struct sched_event *event = NULL;

    event = bpf_map_lookup_elem(&scheds, &cpu);
    if (!event)
        return 0;

    u64 ts = bpf_ktime_get_ns();
    event->ts = ts;
    event->next_pid = arg->next_pid;
    event->prev_pid = arg->prev_pid;

    add_sched_in(arg->next_pid, ts, cpu);
    add_sched_out(arg->prev_pid, ts, cpu);

    __builtin_memcpy(event->next_comm, arg->next_comm, 16);
    __builtin_memcpy(event->prev_comm, arg->prev_comm, 16);

    return 0;
}

struct tp_sched_wakeup_arg
{
    u64 pad;
    char comm[16];
    pid_t pid;
    int prio;
};

SEC("tracepoint/sched/sched_wakeup")
int tp_sched_wakeup(struct tp_sched_wakeup_arg *arg)
{
    u32 tid = arg->pid;
    u64 ts = bpf_ktime_get_ns();
    int cpu = bpf_get_smp_processor_id();
    add_sched_wakeup(tid, ts, cpu);
    return 0;
}

char _license[] SEC("license") = "GPL";