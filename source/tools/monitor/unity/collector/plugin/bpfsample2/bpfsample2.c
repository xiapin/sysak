

#include "bpfsample2.h"
#include "bpfsample2.skel.h"

#include <pthread.h>
#include <coolbpf.h>
#include "../../../../unity/beeQ/beeQ.h"

// ======== User can modify the starting area ======== //

void handle_event(void *ctx, int cpu, void *data, __u32 data_sz)
{
    struct event *e = (struct event *)data;
    struct unity_line *line;
    struct unity_lines *lines = unity_new_lines();
    struct beeQ *q = (struct beeQ *)ctx;

    unity_alloc_lines(lines, 1);
    line = unity_get_line(lines, 0);
    unity_set_table(line, "bpfsample2");
    unity_set_index(line, 0, "index", "value");
    unity_set_value(line, 0, "ns", e->ns);
    unity_set_value(line, 1, "cpu", e->cpu);
    beeQ_send(q, lines);
}

// ======== User can modify the termination area ======== //

DEFINE_SEKL_OBJECT(bpfsample2);

void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt)
{
    printf("Lost %llu events on CPU #%d!\n", lost_cnt, cpu);
}

int thread_worker(struct beeQ *q, void *arg)
{
    perf_thread_worker(arg);
    return 0;
}

int init(void *arg)
{
    printf("bpfsample2 plugin install.\n");
    return LOAD_SKEL_OBJECT(bpfsample2, perf);
}

int call(int t, struct unity_lines *lines)
{
    return 0;
}

void deinit(void)
{
    DESTORY_SKEL_BOJECT(bpfsample2);
}
