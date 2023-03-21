//
// Created by 廖肇燕 on 2023/2/24.
//

#include "net_health.h"
#include "../bpf_head.h"
#include "net_health.skel.h"

#define NET_DIST_INDEX  4
#define DIST_ARRAY_SIZE 20

DEFINE_SEKL_OBJECT(net_health);
static int cnt_fd = 0;
static int dist_fd = 0;

//#define ZHAOYAN_DEBUG

int init(void *arg)
{
    int ret;
    printf("net_health plugin install.\n");
    ret = LOAD_SKEL_OBJECT(net_health, perf);
    cnt_fd = coobpf_map_find(net_health->obj, "outCnt");
    dist_fd = coobpf_map_find(net_health->obj, "netHist");
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
#ifdef ZHAOYAN_DEBUG
    for (i = 0; i < NET_DIST_INDEX; i ++) {
        printf("%ld, ", locals[i]);
    }
    printf("\n");
#endif
    return i;
}

static int cal_dist(unsigned long* values) {
    int i, j;
    int size;
    static unsigned long rec[DIST_ARRAY_SIZE] = {0};
    unsigned long locals[DIST_ARRAY_SIZE];

    size = get_dist(locals);
    for (i = 0; i < NET_DIST_INDEX - 1; i ++) {
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

static int get_count(unsigned long* values) {
    int key;
    static unsigned long rec[2];
    unsigned long now[2];

    key = 0;
    coobpf_key_value(cnt_fd, &key, &now[0]);
    key = 1;
    coobpf_key_value(cnt_fd, &key, &now[1]);

    values[0] = now[0] - rec[0]; rec[0] = now[0];
    values[1] = now[1] - rec[1]; rec[1] = now[1];
    return 0;
}

int call(int t, struct unity_lines *lines)
{
    int i;
    unsigned long values[NET_DIST_INDEX];
    const char *names[] = { "ms10", "ms100", "s1", "so"};
    struct unity_line* line;

    unity_alloc_lines(lines, 2);    // 预分配好
    line = unity_get_line(lines, 0);
    unity_set_table(line, "net_health_hist");

    cal_dist(values);
    for (i = 0; i < NET_DIST_INDEX; i ++ ) {
        unity_set_value(line, i, names[i], values[i]);
    }

    get_count(values);
    line = unity_get_line(lines, 1);
    unity_set_table(line, "net_health_count");
    unity_set_value(line, 0, "sum", values[0]);
    unity_set_value(line, 1, "count", values[1]);
    return 0;
}

void deinit(void)
{
    printf("net_health plugin uninstall.\n");
    DESTORY_SKEL_BOJECT(net_health);
}


