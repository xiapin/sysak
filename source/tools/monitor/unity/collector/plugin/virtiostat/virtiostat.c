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
    stats_fd = coobpf_map_find(virtiostat->obj, "stats");
    return ret;
}

void walk_virtio(void) {
    unsigned long key, next;
    struct virtio_stat stat;

    key = 0;
    while (coobpf_key_next(stats_fd, &key, &next) == 0) {
        bpf_map_lookup_elem(stats_fd, &next, &stat);
        key = next;
        printf("driver:%s dev:%s, name:%s, in:%d, out:%d\n", stat.driver, stat.dev, stat.vqname, stat.in_sgs, stat.out_sgs);
    }
}

int call(int t, struct unity_lines *lines) {
    printf("call 2.\n");
    walk_virtio();
    return 0;
}

void deinit(void)
{
    printf("virtiostat plugin uninstall.\n");
    DESTORY_SKEL_BOJECT(virtiostat);
}
