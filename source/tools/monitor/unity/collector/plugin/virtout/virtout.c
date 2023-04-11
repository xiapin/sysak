//
// Created by 廖肇燕 on 2023/2/23.
//

#include <unistd.h>
#define COOLBPF_PERF_THREAD
#include "../bpf_head.h"
#include "virtout.h"
#include "virtout.skel.h"

#include <string.h>
#include <stdio.h>
#include <linux/perf_event.h>

#define CPU_DIST_INDEX  4
#define DIST_ARRAY_SIZE 20

static volatile int budget = 0;   // for log budget
static int dist_fd = 0;
static int stack_fd = 0;
int proc(int stack_fd, struct data_t *e, struct unity_line *line);
void handle_event(void *ctx, int cpu, void *data, __u32 data_sz)
{
    int ret;
    if (budget > 0) {
        struct data_t *e = (struct data_t *)data;
        struct beeQ *q = (struct beeQ *)ctx;
        struct unity_line *line;
        struct unity_lines *lines = unity_new_lines();

        unity_alloc_lines(lines, 1);
        line = unity_get_line(lines, 0);
        ret = proc(stack_fd, e, line);
        if (ret >= 0) {
            beeQ_send(q, lines);
        }
        budget --;
    }
}

DEFINE_SEKL_OBJECT(virtout);
int init(void *arg) {
    int ret;
    printf("virtout plugin install.\n");

    ret = LOAD_SKEL_OBJECT(virtout, perf);
    dist_fd = coobpf_map_find(virtout->obj, "virtdist");
    stack_fd = coobpf_map_find(virtout->obj, "stack");
    return ret;
}

static int get_dist(unsigned long *locals) {
    int i = 0;
    unsigned long value = 0;
    int key, key_next;

    key = 0;
    while (coobpf_key_next(dist_fd, &key, &key_next) == 0) {
        coobpf_key_value(dist_fd, &key_next, &value);
        locals[i ++] = value;
        if (i > DIST_ARRAY_SIZE) {
            break;
        }
        key = key_next;
    }
    return i;
}

static int cal_dist(unsigned long* values) {
    int i, j;
    int size;
    static unsigned long rec[DIST_ARRAY_SIZE] = {0};
    unsigned long locals[DIST_ARRAY_SIZE];

    size = get_dist(locals);
    for (i = 0; i < CPU_DIST_INDEX - 1; i ++) {
        values[i] = locals[i] - rec[i];
        rec[i] = locals[i];
    }
    j = i;
    values[j] = 0;
    for (; i < size; i ++) {
        values[j] += locals[i] - rec[i];
        rec[i] = locals[i];
    }
    return 0;
}

int call(int t, struct unity_lines *lines) {
    int i;
    unsigned long values[CPU_DIST_INDEX];
    const char *names[] = { "ms100", "s1", "s10", "so"};
    struct unity_line* line;

    budget = t;

    unity_alloc_lines(lines, 1);    // 预分配好
    line = unity_get_line(lines, 0);
    unity_set_table(line, "virtout_dist");

    cal_dist(values);
    for (i = 0; i < CPU_DIST_INDEX; i ++ ) {
        unity_set_value(line, i, names[i], values[i]);
    }
    return 0;
}


void deinit(void)
{
    printf("virout plugin uninstall.\n");
    DESTORY_SKEL_BOJECT(virtout);
}

#define LOG_MAX 256
static char log[LOG_MAX];

int proc(int stack_fd, struct data_t *e, struct unity_line *line) {
    int i;
    unsigned long addr[128];
    int id = e->stack_id;  //last stack
    struct ksym_cell* cell;

    snprintf(log, LOG_MAX, "task:%d(%s), cpu:%d, delayed:%ld, callstack:", e->pid, e->comm, e->cpu, e->delta);
    coobpf_key_value(stack_fd, &id, &addr);

    for (i = 0; i < 128; i ++) {
        if (addr[i] > 0) {
            cell = ksym_search(addr[i]);
            strncpy(log, cell->func, LOG_MAX);
            strncpy(log, ",", LOG_MAX);
        } else {
            break;
        }
    }
    unity_set_table(line, "virtout_log");
    unity_set_log(line, "log", log);
    return 0;
}
