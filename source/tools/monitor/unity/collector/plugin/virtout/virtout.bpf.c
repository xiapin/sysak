#include <vmlinux.h>
#include <coolbpf.h>
#include "virtout.h"

static inline int fast_log2(long value)
{
    int n = 0;
    int i;

    if (value < 1) {
        goto end;
    }

#pragma unroll
    for (i = 32; i > 0; i /= 2) {
        long v = 1ULL << i;
        if (value >= v) {
            n += i;
            value = value >> i;
        }
    }
    end:
    return n;
}

#define NUM_E16 10000000000000000ULL
#define NUM_E8  100000000ULL
#define NUM_E4  10000ULL
#define NUM_E2  100ULL
#define NUM_E1  10ULL
static inline int fast_log10(long v)
{
    int n = 0;
    if (v >= NUM_E16) {n += 16; v /= NUM_E16;}
    if (v >=  NUM_E8) {n +=  8; v /=  NUM_E8;}
    if (v >=  NUM_E4) {n +=  4; v /=  NUM_E4;}
    if (v >=  NUM_E2) {n +=  2; v /=  NUM_E2;}
    if (v >=  NUM_E1) {n +=  1;}
    return n;
}

static inline void add_hist(struct bpf_map_def* maps, int k, int v) {
    u64 *pv = bpf_map_lookup_elem(maps, &k);
    if (pv) {
        __sync_fetch_and_add(pv, v);
    }
}

#define incr_hist(maps, k) add_hist(maps, k, 1)

static inline void hist2_push(struct bpf_map_def* maps, long v) {
    int k = fast_log2(v);
    incr_hist(maps, k);
}

static inline void hist10_push(struct bpf_map_def* maps, long v) {
    int k = fast_log10(v);
    incr_hist(maps, k);
}

#define LBC_HIST10(MAPS) \
    struct bpf_map_def SEC("maps") MAPS = { \
        .type = BPF_MAP_TYPE_ARRAY, \
        .key_size = sizeof(int), \
        .value_size = sizeof(long), \
        .max_entries = 20, \
    }

#define PERIOD_TIME (10 * 1000 * 1000ULL)
#define THRESHOLD_TIME (200 * 1000 * 1000ULL)

struct bpf_map_def SEC("maps") virtHist = { \
        .type = BPF_MAP_TYPE_ARRAY, \
        .key_size = sizeof(int), \
        .value_size = sizeof(long), \
        .max_entries = 20, \
    };

struct bpf_map_def SEC("maps") e_sw = { \
        .type = BPF_MAP_TYPE_PERF_EVENT_ARRAY, \
        .key_size = sizeof(int), \
        .value_size = sizeof(struct data_t), \
        .max_entries = 16, \

};
struct bpf_map_def SEC("maps") call_stack = { \
        .type = BPF_MAP_TYPE_STACK_TRACE, \
        .key_size = sizeof(u32), \
        .value_size = PERF_MAX_STACK_DEPTH * sizeof(u64), \
        .max_entries = 1024, \
    };

struct bpf_map_def SEC("maps") perRec = { \
        .type = BPF_MAP_TYPE_PERCPU_ARRAY, \
        .key_size = sizeof(int), \
        .value_size = sizeof(u64), \
        .max_entries = 2, \
    };

static void store_con(char* con, struct task_struct *p)
{
    struct cgroup_name *cname;
    cname = BPF_CORE_READ(p, cgroups, subsys[0], cgroup, name);
    if (cname != NULL) {
        bpf_core_read(con, CON_NAME_LEN, &cname->name[0]);
    } else {
        con[0] = '\0';
    }
}

static inline u64 get_last(int index) {
    u64 *pv = bpf_map_lookup_elem(&perRec, &index);
    if (pv) {
        return *pv;
    }
    return 0;
}

static inline void save_last(int index, u64 ns) {
    bpf_map_update_elem(&perRec, &index, &ns, BPF_ANY);
}

static inline void check_time(struct bpf_perf_event_data *ctx,
                              int index,
                              struct bpf_map_def* event) {
    u64 ns = bpf_ktime_get_ns();
    u64 last = get_last(index);
    s64 delta;

    save_last(index, ns);
    delta = ns - last;

    if (last && delta > 2 * PERIOD_TIME) {
        delta -= PERIOD_TIME;
        hist10_push(&virtHist, delta / PERIOD_TIME);
        if (delta >= THRESHOLD_TIME) {
            struct task_struct* task = bpf_get_current_task();
            struct data_t data = {};

            data.pid = bpf_get_current_pid_tgid() >> 32;
            data.stack_id = bpf_get_stackid(ctx, &call_stack, KERN_STACKID_FLAGS);
            data.delta = delta;
            bpf_get_current_comm(&data.comm, TASK_COMM_LEN);
            store_con(&data.con[0], task);

            bpf_perf_event_output(ctx, event, BPF_F_CURRENT_CPU, &data, sizeof(data));
        }
    }
}

SEC("perf_event")
int sw_clock(struct bpf_perf_event_data *ctx)
{
    check_time(ctx, 0, &e_sw);
    return 0;
}