//
// Created by 廖肇燕 on 2023/2/23.
//


#include "cpudist.h"
#include "../bpf_head.h"
#include "cpudist.skel.h"

#define CPU_DIST_INDEX  8
#define DIST_ARRAY_SIZE 20
DEFINE_SEKL_OBJECT(cpudist);
static int dist_fd = 0;

int init(void *arg)
{
    int ret;
    printf("cpudist plugin install.\n");
    ret = LOAD_SKEL_OBJECT(cpudist, perf);
    if (ret >= 0) {
        dist_fd = coobpf_map_find(cpudist->obj, "cpudist");
    }
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


int call(int t, struct unity_lines *lines)
{
    int i;
    unsigned long values[CPU_DIST_INDEX];
    const char *names[] = {"us1", "us10", "us100", "ms1", "ms10", "ms100", "s1", "so"};
    struct unity_line* line;

    unity_alloc_lines(lines, 1);    // 预分配好
    line = unity_get_line(lines, 0);
    unity_set_table(line, "cpu_dist");

    cal_dist(values);
    for (i = 0; i < CPU_DIST_INDEX; i ++ ) {
        unity_set_value(line, i, names[i], values[i]);
    }

    return 0;
}

void deinit(void)
{
    printf("cpudist plugin uninstall.\n");
    DESTORY_SKEL_BOJECT(cpudist);
}

