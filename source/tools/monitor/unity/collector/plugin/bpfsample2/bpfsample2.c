

#include "bpfsample2.h"
#include "bpfsample2.skel.h"

#include <pthread.h>
#include <coolbpf.h>
#include "../../../../unity/beeQ/beeQ.h"


static pthread_t perf_thread = 0;

void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt)
{
    printf("Lost %llu events on CPU #%d!\n", lost_cnt, cpu);
}

void handle_event(void *ctx, int cpu, void *data, __u32 data_sz)
{
    struct event *e = (struct event *)data;
    struct unity_line *line;
    struct unity_lines *lines;

    unity_alloc_lines(lines, 1);
    line = unity_get_line(lines, 0);
    unity_set_table(line, "bpfsample");
    unity_set_index(line, 0, "index", "value");
    unity_set_value(line, 0, "ns", e->ns);
    unity_set_value(line, 1, "cpu", e->cpu);
}

#define LOAD_BPF_SKEL(name)                                                    \
    (                                                                          \
        {                                                                      \
            __label__ load_bpf_skel_out;                                       \
            int __ret = 0;                                                     \
            name = name##_bpf__open();                                         \
            if (!name)                                                         \
            {                                                                  \
                printf("failed to open BPF object\n");                         \
                __ret = -1;                                                    \
                goto load_bpf_skel_out;                                        \
            }                                                                  \
            __ret = name##_bpf__load(name);                                    \
            if (__ret)                                                         \
            {                                                                  \
                printf("failed to load BPF object: %d\n", err);                \
                goto load_bpf_skel_out;                                        \
            }                                                                  \
            __ret = name##_bpf__attach(name);                                  \
            if (__ret)                                                         \
            {                                                                  \
                printf("failed to attach BPF programs: %s\n", strerror(-err)); \
                goto load_bpf_skel_out;                                        \
            }                                                                  \
        load_bpf_skel_out:                                                     \
            __ret;                                                             \
        })


int thread_worker(struct beeQ *q, void *arg) {
    perf_thread_worker(arg);
    return 0;
}

int init(void *arg)
{

    struct bpfsample2_bpf *bpfsample2 = NULL;
    int err;
    printf("coolbpf library version: %s\n", coolbpf_version_string());

    err = LOAD_BPF_SKEL(bpfsample2);
    if (err)
        return err;

    printf("bpfsample program load done.\n");
    struct perf_thread_arguments perf_args = {};

    perf_args.mapfd = bpf_map__fd(bpfsample2->maps.perf);
    perf_args.sample_cb = handle_event;
    perf_args.lost_cb = handle_lost_events;

    beeQ_send_thread(arg, &perf_args, thread_worker);

    return 0;
}

int call(int t, struct unity_lines *lines)
{
    return 0;
}

void deinit(void)
{
    kill_perf_thread(perf_thread);
}
