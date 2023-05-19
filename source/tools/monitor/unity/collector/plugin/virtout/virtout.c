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
#include <sys/syscall.h>
#include <errno.h>


#define CPU_DIST_INDEX  4
#define DIST_ARRAY_SIZE 20

static volatile int budget = 0;   // for log budget
static int dist_fd = 0;
static int stack_fd = 0;
static int perf_fds[1024];

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

static void close_perf_fds(void) {
    int i;
    for (i = 0; i < 1024; i ++) {
        if (perf_fds[i] > 0) {
            close(perf_fds[i]);
            perf_fds[i] = 0;
        }
    }
}

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                            int cpu, int group_fd, int flags)
{
    int ret;

    ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
                  group_fd, flags);
    return ret;
}

DEFINE_SEKL_OBJECT(virtout);
static struct bpf_prog_skeleton *search_progs(const char* func) {
    struct bpf_object_skeleton *s;
    int i;

    s = virtout->skeleton;
    for (i = 0; i < s->prog_cnt; i ++) {
        if (strcmp(s->progs[i].name, func) == 0) {
            return &(s->progs[i]);
        }
    }
    return NULL;
}

static int setup_perf_events(const char* func) {
    int nr_cpus = libbpf_num_possible_cpus();
    int i;
    struct bpf_link *link;
    struct bpf_prog_skeleton *progs;

    progs = search_progs(func);

    struct perf_event_attr perf_attr = {
            .type = PERF_TYPE_SOFTWARE,
            .config = PERF_COUNT_SW_CPU_CLOCK,
            .freq = 0,
            .sample_period = 10 * 1000 * 1000,
    };

    for (i = 0; i < nr_cpus; i ++) {
        perf_fds[i] = perf_event_open(&perf_attr, -1, i, -1, 0);
        if (perf_fds[i] < 0) {
            if (errno == ENODEV) {
                printf("skip offline cpu id: %d\n", i);
                continue;
            } else {
                perror("syscall failed.");
                close_perf_fds();
                return -1;
            }
        }

        link = bpf_program__attach_perf_event(*(progs->prog), perf_fds[i]);
        if (!link) {
            perror("attach failed.");
            close_perf_fds();
            return -1;
        }
        *(progs->link) = link;
    }
    printf("setup virout OK.\n");
    return 0;
}

int init(void *arg) {
    int ret;
    printf("virtout plugin install.\n");

    ret = LOAD_SKEL_OBJECT(virtout, perf);
    if (ret < 0) {
        return  ret;
    }
    ret = setup_perf_events("sw_clock");
    if (ret < 0) {
        return  ret;
    }
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
        locals[key] = value;
        if (key > DIST_ARRAY_SIZE) {
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
    unsigned long locals[DIST_ARRAY_SIZE] = {0};

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
    close_perf_fds();
    DESTORY_SKEL_BOJECT(virtout);
}

#define LOG_MAX 4096
int proc(int stack_fd, struct data_t *e, struct unity_line *line) {
    int i;
    char log[LOG_MAX] = {'\0'};
    unsigned long addr[128];
    int id = e->stack_id;  //last stack
    struct ksym_cell* cell;

    snprintf(log, LOG_MAX, "task:%d(%s);us:%ld;cpu:%d;delayed:%ld;callstack:", e->pid, e->comm, e->us, e->cpu, e->delta);
    coobpf_key_value(stack_fd, &id, &addr);

    for (i = 0; i < 128; i ++) {
        if (addr[i] > 0) {
            cell = ksym_search(addr[i]);
            if (cell != NULL) {
                strncat(log, cell->func, LOG_MAX - 1 - strlen(log));
            } else {
                strncat(log, "!nil", LOG_MAX - - 1 - strlen(log));
            }
            strncat(log, ",", LOG_MAX - 1 - strlen(log));
        } else {
            break;
        }
    }
    unity_set_table(line, "virtout_log");
    unity_set_log(line, "log", log);
    return 0;
}
