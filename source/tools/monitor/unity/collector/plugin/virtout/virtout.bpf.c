//
// Created by 廖肇燕 on 2023/2/23.
//

#include <vmlinux.h>
#include <coolbpf.h>
#include "virtout.h"

#define MAX_ENTRY 128
#define BPF_F_FAST_STACK_CMP	(1ULL << 9)
#define KERN_STACKID_FLAGS	(0 | BPF_F_FAST_STACK_CMP)

#define PERIOD_TIME (10 * 1000 * 1000ULL)
#define THRESHOLD_TIME (200 * 1000 * 1000ULL)

BPF_ARRAY(virtdist, u64, 20);
BPF_PERF_OUTPUT(perf, 1024);
BPF_STACK_TRACE(stack, MAX_ENTRY);
BPF_PERCPU_ARRAY(perRec, u64, 2);

static inline u64 get_last(int index) {
    u64 *pv = bpf_map_lookup_elem(&perRec, &index);
    if (pv) {
        return *pv;
    }
    return 0;
}

static inline void save_last(int index, u64 t) {
    bpf_map_update_elem(&perRec, &index, &t, BPF_ANY);
}

static inline void check_time(struct bpf_perf_event_data *ctx,
                              int index,
                              struct bpf_map* event) {
    u64 t = ns();
    u64 last = get_last(index);
    s64 delta;

    save_last(index, t);
    delta = t - last;

    if (last && delta > 2 * PERIOD_TIME) {
        delta -= PERIOD_TIME;
        hist10_push((struct bpf_map_def *)&virtdist, delta / PERIOD_TIME);
        if (delta >= THRESHOLD_TIME) {
            struct task_struct* task = (struct task_struct *)bpf_get_current_task();
            struct data_t data = {};

            data.pid = pid();
            data.stack_id = bpf_get_stackid(ctx, &stack, KERN_STACKID_FLAGS);
            data.delta = delta;
            data.cpu = cpu();
            bpf_get_current_comm(&data.comm, TASK_COMM_LEN);

            bpf_perf_event_output(ctx, event, BPF_F_CURRENT_CPU, &data, sizeof(data));
        }
    }
}

SEC("perf_event")
int sw_clock(struct bpf_perf_event_data *ctx)
{
    check_time(ctx, 1, (struct bpf_map *)&perf);
    return 0;
}
