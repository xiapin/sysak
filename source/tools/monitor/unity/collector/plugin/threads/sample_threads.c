//
// Created by 廖肇燕 on 2022/12/30.
//

#include "sample_threads.h"
#include <unistd.h>
#include <signal.h>

static volatile pthread_t sample_thread_id = 0;

static int sample_thread_func(struct beeQ* q, void * arg);
int init(void * arg) {
    struct beeQ* q = (struct beeQ *)arg;
    sample_thread_id = beeQ_send_thread(q, NULL, sample_thread_func);
    printf("start sample_thread_id: %lu\n", sample_thread_id);
    return 0;
}

static int sample_thread_func(struct beeQ* q, void * arg) {
    while (plugin_is_working()) {
        static double value = 1.0;
        struct unity_line* line;
        struct unity_lines * lines = unity_new_lines();

        unity_alloc_lines(lines, 1);
        line = unity_get_line(lines, 0);
        unity_set_table(line, "sample_tbl3");
        unity_set_value(line, 0, "value1", 1.0 + value);
        unity_set_value(line, 1, "value2", 2.0 + value);
        unity_set_log(line, "log", "hello world.");
        beeQ_send(q, lines);
        sleep(1);
    }
    return 0;
}

int call(int t, struct unity_lines* lines) {
    static double value = 0.0;
    struct unity_line* line;

    unity_alloc_lines(lines, 1);
    line = unity_get_line(lines, 0);
    unity_set_table(line, "sample_tbl1");
    unity_set_index(line, 0, "mode", "threads");
    unity_set_value(line, 0, "value1", 1.0 + value);
    unity_set_value(line, 1, "value2", 2.0 + value);

    value += 0.1;
    return 0;
}

void deinit(void) {
    plugin_thread_stop(sample_thread_id);
    printf("thread plugin uninstall\n");
}
