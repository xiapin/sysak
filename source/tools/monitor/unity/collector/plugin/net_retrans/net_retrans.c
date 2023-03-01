//
// Created by 廖肇燕 on 2023/2/24.
//

#include "net_retrans.h"
#define COOLBPF_PERF_THREAD
#include "../bpf_head.h"
#include "net_retrans.skel.h"

#include <string.h>
#include <stdio.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static volatile int budget = 0;   // for log budget
static int cnt_fd = 0;
static int stack_fd = 0;

const char *net_title[] = {"rto_retrans", "zero_probe", \
                           "noport_reset", "bad_sync", \
                           "net_proc", "syn_send", "syn_ack"};

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

DEFINE_SEKL_OBJECT(net_retrans);
int init(void *arg)
{
    int ret;
    printf("net_retrans plugin install.\n");

    ret = LOAD_SKEL_OBJECT(net_retrans, perf);
    cnt_fd = coobpf_map_find(net_retrans->obj, "outCnt");
    stack_fd = coobpf_map_find(net_retrans->obj, "stack");
    return ret;
}

static int get_count(unsigned long *locals) {
    int i = 0;

    for (i = 0; i < NET_RETRANS_TYPE_MAX; i ++) {
        coobpf_key_value(cnt_fd, &i, &locals[i]);
    }
    return i;
}

static int cal_retrans(unsigned long *values) {
    int i;
    static unsigned long rec[NET_RETRANS_TYPE_MAX] = {0};
    unsigned long locals[NET_RETRANS_TYPE_MAX];

    get_count(locals);
    for (i = 0; i < NET_RETRANS_TYPE_MAX; i ++) {
        values[i] = locals[i] - rec[i];
        rec[i] = locals[i];
    }
    return 0;
}

int call(int t, struct unity_lines *lines) {
    int i;
    unsigned long values[NET_RETRANS_TYPE_MAX];
    struct unity_line* line;

    budget = t;   //release log budget

    unity_alloc_lines(lines, 1);    // 预分配好
    line = unity_get_line(lines, 0);
    unity_set_table(line, "net_retrans_count");

    cal_retrans(values);
    for (i = 0; i < NET_RETRANS_TYPE_MAX; i ++) {
        unity_set_value(line, i, net_title[i], values[i]);
    }

    return 0;
}

void deinit(void)
{
    printf("net_retrans plugin uninstall.\n");
    DESTORY_SKEL_BOJECT(net_retrans);
}

#define LOG_MAX 256
static char log[LOG_MAX];

static int transIP(unsigned long lip, char *result, int size) {
    inet_ntop(AF_INET, (void *) &lip, result, size);
    return 0;
}

static const char * resetSock(int stack_fd, struct data_t *e){
    unsigned long addr;
    int i = 1;  //last stack
    struct ksym_cell* cell;

    coobpf_key_value(stack_fd, &i, &addr);
    if (addr) {
        cell = ksym_search(addr);
        if (cell) {
            if (strcmp(cell->func, "tcp_v4_rcv") == 0) {
                if (e->sk_state == 12) {
                    return "bad_ack";   // TCP_NEW_SYN_REC
                } else {
                    return "tw_rst";
                }
            } else if (strcmp(cell->func, "tcp_check_req") == 0) {
                return "bad_syn";
            } else if (strcmp(cell->func, "tcp_v4_do_rcv") == 0) {
                return "tcp_stat";
            } else {
                return "unknown_sock";
            }
        }
    }
    return "failure_sock";
}

static const char * resetActive(int stack_fd, struct data_t *e){
    unsigned long addr;
    int i = 1;  //last stack
    struct ksym_cell* cell;

    coobpf_key_value(stack_fd, &i, &addr);
    if (addr) {
        cell = ksym_search(addr);
        if (cell) {
            if (strcmp(cell->func, "tcp_out_of_resources") == 0) {
                return "tcp_oom";
            } else if (strcmp(cell->func, "tcp_keepalive_timer") == 0) {
                return "keep_alive";
            } else if (strcmp(cell->func, "inet_release") == 0) {
                return "bad_close";
            } else if (strcmp(cell->func, "tcp_close") == 0) {
                return "bad_close";
            } else if (strcmp(cell->func, "tcp_disconnect") == 0) {
                return "tcp_abort";
            } else if (strcmp(cell->func, "tcp_abort") == 0) {
                return "tcp_abort";
            } else {
                return "unknown_active";
            }
        }
    }
    return "failure_active";
}

int proc(int stack_fd, struct data_t *e, struct unity_line *line) {
    char sip[32];
    char dip[32];

    transIP(e->ip_src, sip, 32);
    transIP(e->ip_dst, dip, 32);
    snprintf(log, LOG_MAX, "task:%d|%s, tcp:%s:%d->%s:%d, state:%d, ", e->pid, e->comm, \
             sip, htons(e->sport),   \
             dip, htons(e->sport),   \
             e->sk_state);
    switch (e->type) {
        case NET_RETRANS_TYPE_RTO:
        case NET_RETRANS_TYPE_ZERO:
        case NET_RETRANS_TYPE_SYN:
        case NET_RETRANS_TYPE_SYN_ACK:
        {
            char buf[LOG_MAX - 1];
            snprintf(buf, LOG_MAX - 1, "rcv_nxt:%d, rcv_wup:%d, snd_nxt:%d, snd_una:%d, copied_seq:%d, "
                                   "snd_wnd:%d, rcv_wnd:%d, lost_out:%d, packets_out:%d, retrans_out:%d, "
                                   "sacked_out:%d, reordering:%d",
                     e->rcv_nxt, e->rcv_wup, e->snd_nxt, e->snd_una, e->copied_seq,
                     e->snd_wnd, e->rcv_wnd, e->lost_out, e->packets_out, e->retrans_out,
                     e->sacked_out, e->reordering
            );
            strncat(log, buf, LOG_MAX -1);
        }
            break;
        case NET_RETRANS_TYPE_RST:
            strncat(log, "noport", LOG_MAX - 1);
            break;
        case NET_RETRANS_TYPE_RST_SK:
        {
            const char *type = resetSock(stack_fd, e);
            strncat(log, type, LOG_MAX - 1);
        }
            break;
        case NET_RETRANS_TYPE_RST_ACTIVE:
        {
            const char *type = resetActive(stack_fd, e);
            strncat(log, type, LOG_MAX - 1);
        }
            break;
        default:
            break;
    }
    unity_set_table(line, "net_retrans_log");
    unity_set_index(line, 0, "type", net_title[e->type]);
    unity_set_log(line, "log", log);
    return 0;
}
