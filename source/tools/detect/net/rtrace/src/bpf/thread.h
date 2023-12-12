

#ifndef __THREAD_H
#define __THREAD_H

#define MAX_ITEM_NUM 16

enum THREAD_ITEM_TYPE
{
    SCHED_IN,
    SCHED_OUT,
    WAKE_UP,
    MIGRATION,
};

struct thread_item
{
    unsigned short ty;
    unsigned short cpu;
    unsigned long long ts;
};

struct thread_event
{
    unsigned int tid;
    unsigned int cnt;
    struct thread_item items[MAX_ITEM_NUM];
};

#ifdef __VMLINUX_H__

struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 102400);
    __type(key, u32);
    __type(value, struct thread_event);
} threads SEC(".maps");

static __always_inline void __add_thread_item(struct thread_event *event, enum THREAD_ITEM_TYPE ty, u64 ts, u16 cpu)
{
    u64 cnt = event->cnt & (MAX_ITEM_NUM - 1);
    event->cnt++;
    event->items[cnt].ty = ty;
    event->items[cnt].ts = ts;
    event->items[cnt].cpu = cpu;
}

static __always_inline void add_thread_item(u32 tid, enum THREAD_ITEM_TYPE ty, u64 ts, u16 cpu)
{
    struct thread_event *eventp;
    eventp = bpf_map_lookup_elem(&threads, &tid);
    if (!eventp)
    {
        struct thread_event event = {0};
        __add_thread_item(&event, ty, ts, cpu);
        bpf_map_update_elem(&threads, &tid, &event, BPF_ANY);
    }
    else
    {
        __add_thread_item(eventp, ty, ts, cpu);
    }
}

static __always_inline void add_sched_in(u32 tid, u64 ts, u16 cpu)
{
    add_thread_item(tid, SCHED_IN, ts, cpu);
}

static __always_inline void add_sched_out(u32 tid, u64 ts, u16 cpu)
{
    add_thread_item(tid, SCHED_OUT, ts, cpu);
}

static __always_inline void add_sched_wakeup(u32 tid, u64 ts, u16 cpu)
{
    add_thread_item(tid, WAKE_UP, ts, cpu);
}

#endif

#endif