//
// Created by 廖肇燕 on 2023/4/11.
//

#include "sum_retrans.h"
#include "../bpf_head.h"
#include "sum_retrans.skel.h"

#include <string.h>
#include <stdio.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


DEFINE_SEKL_OBJECT(sum_retrans);
static int inum_fd = 0;
static int dip_fd = 0;
int init(void *arg)
{
    int ret;
    printf("sum_retrans plugin install.\n");
    ret = LOAD_SKEL_OBJECT(sum_retrans, perf);
    if (ret >= 0) {
        inum_fd = coobpf_map_find(sum_retrans->obj, "inums");
        dip_fd = coobpf_map_find(sum_retrans->obj, "dips");
    }
    return ret;
}

#define BUFF_SIZE 64
#define LOG_MAX 4096
static char log[LOG_MAX];

static int transIP(unsigned long lip, char *result, int size) {
    inet_ntop(AF_INET, (void *) &lip, result, size);
    return 0;
}

static void pack_dip() {
    char ips[32];
    char buff[BUFF_SIZE];
    unsigned long value;
    unsigned int ip, ip_next;

    log[0] = '\0';
    ip = 0;
    while (coobpf_key_next(dip_fd, &ip, &ip_next) == 0) {
        bpf_map_lookup_elem(dip_fd, &ip_next, &value);
        ip = ip_next;

        transIP(ip, ips, 32);
        snprintf(buff, BUFF_SIZE, "%s:%ld,", ips, value);
        strncat(log, buff, LOG_MAX - 1 - strlen(log));
        ip = ip_next;
    }

    ip = 0;
    while (coobpf_key_next(dip_fd, &ip, &ip_next) == 0) {
        bpf_map_delete_elem(dip_fd, &ip_next);
        ip = ip_next;
    }
}

static void pack_inum() {
    char buff[BUFF_SIZE];
    unsigned long value;
    unsigned int inum, inum_next;

    log[0] = '\0';

    inum = 0;
    while (coobpf_key_next(inum_fd, &inum, &inum_next) == 0) {
        bpf_map_lookup_elem(inum_fd, &inum_next, &value);
        snprintf(buff, BUFF_SIZE, "%u:%ld,", inum_next, value);
        strncat(log, buff, LOG_MAX - 1 - strlen(log));
        inum = inum_next;
    }

    inum = 0;
    while (coobpf_key_next(inum_fd, &inum, &inum_next) == 0) {
        bpf_map_delete_elem(inum_fd, &inum_next);
        inum = inum_next;
    }
}

int call(int t, struct unity_lines *lines)
{
    struct unity_line* line;

    pack_dip();
    if (strlen(log) > 0) {
        unity_alloc_lines(lines, 2);    // 预分配好

        line = unity_get_line(lines, 0);
        unity_set_table(line, "retrans_dip");
        unity_set_log(line, "ip_log", log);

        pack_inum();
        line = unity_get_line(lines, 1);
        unity_set_table(line, "retrans_inum");
        unity_set_log(line, "inum_log", log);
    }

    return 0;
}

void deinit(void)
{
    printf("sum_retrans plugin uninstall.\n");
    DESTORY_SKEL_BOJECT(sum_retrans);
}
