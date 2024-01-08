//
// Created by 廖肇燕 on 2023/8/1.
//

#include "rto_accel.h"
#include "../bpf_head.h"
#include "rto_accel.skel.h"

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#define NS_FDS_NUM 256

static int ns_fds[NS_FDS_NUM] = {0};

static void close_ns_fds(void) {
    int i;
    for (i = 0; i < NS_FDS_NUM; i ++) {
        if (ns_fds[i] > 0) {
            close(ns_fds[i]);
            ns_fds[i] = 0;
        }
    }
}

DEFINE_SEKL_OBJECT(rto_accel);
static struct bpf_prog_skeleton *search_progs(const char* func) {
    struct bpf_object_skeleton *s;
    int i;

    s = rto_accel->skeleton;
    for (i = 0; i < s->prog_cnt; i ++) {
        if (strcmp(s->progs[i].name, func) == 0) {
            return &(s->progs[i]);
        }
    }
    return NULL;
}

static int combine() {
    return 0;
}

int init(void *arg)
{
    int ret = 0;
    printf("rto_accel plugin install.\n");

    return ret;
}

int call(int t, struct unity_lines *lines)
{
    struct unity_line* line;

    unity_alloc_lines(lines, 1);    // 预分配好
    line = unity_get_line(lines, 0);
    unity_set_table(line, "rto_accel");
    unity_set_value(line, 0, "value1", 1);

    return 0;
}

void deinit(void)
{
    printf("rto_accel plugin uninstall.\n");
    DESTORY_SKEL_BOJECT(rto_accel);
}
