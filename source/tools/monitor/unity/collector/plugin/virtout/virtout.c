//
// Created by 廖肇燕 on 2023/2/7.
//

#include "virtout.h"
#include "virtout.skel.h"

#include <pthread.h>
#include <coolbpf.h>

DEFINE_SEKL_OBJECT(virtout);

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

void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt)
{
    printf("Lost %llu events on CPU #%d!\n", lost_cnt, cpu);
}

int init(void *arg)
{
    printf("bpfsample2 plugin install.\n");
    return LOAD_SKEL_OBJECT(virtout, e_sw);
}

int call(int t, struct unity_lines *lines)
{

}

void deinit(void)
{
    DESTORY_SKEL_BOJECT(virtout);
}

