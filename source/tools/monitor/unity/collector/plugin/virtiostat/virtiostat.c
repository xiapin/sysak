//
// Created by 廖肇燕 on 2023/5/7.
//

#include "virtiostat.h"
#include "../bpf_head.h"
#include "virtiostat.skel.h"

DEFINE_SEKL_OBJECT(virtiostat);
static int stats_fd = 0;

int init(void *arg)
{
    int ret;
    printf("virtiostat plugin install.\n");
    ret = LOAD_SKEL_OBJECT(virtiostat, perf);
    if (ret >= 0) {
        stats_fd = coobpf_map_find(virtiostat->obj, "stats");
    }
    return ret;
}

#define BUF_MAX 128
#define LOG_MAX 4096
void walk_virtio(struct unity_lines *lines) {
    struct unity_line *line;
    unsigned long key, next;
    struct virtio_stat stat;
    char log[LOG_MAX] = {'\0'};

    key = 0;
    while (coobpf_key_next(stats_fd, &key, &next) == 0) {
        char buf[BUF_MAX];
        bpf_map_lookup_elem(stats_fd, &next, &stat);
        key = next;
        snprintf(buf, BUF_MAX, "driver:%s,dev:%s,name:%s,in:%d,out:%d;", stat.driver, stat.dev, stat.vqname, stat.in_sgs, stat.out_sgs);
        strncat(log, buf, LOG_MAX - 1 - strlen(log));
    }
    unity_alloc_lines(lines, 1);
    line = unity_get_line(lines, 0);
    unity_set_table(line, "virtios");
    unity_set_log(line, "log", log);
}

int call(int t, struct unity_lines *lines) {
    walk_virtio(lines);
    return 0;
}

void deinit(void)
{
    printf("virtiostat plugin uninstall.\n");
    DESTORY_SKEL_BOJECT(virtiostat);
}
