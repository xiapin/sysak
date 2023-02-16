//
// Created by 廖肇燕 on 2022/12/26.
//

#include "beeQ.h"
#include "apps.h"
#include "beaver.h"
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define RUN_THREAD_MAX  8
#define RUN_QUEUE_SIZE  32

volatile int sighup_counter = 0;
char *g_yaml_file = NULL;

void sig_handler(int num)
{
    printf("receive the signal %d.\n", num);
    if (num == SIGHUP) {
        sighup_counter ++;
    } else {
        exit(1);
    }
}

extern struct beeQ* proto_sender_init(struct beeQ* pushQ);
int main(int argc, char *argv[]) {
    struct beeQ* q;
    struct beeQ* proto_que;

    if (argc > 1) {
        g_yaml_file = argv[1];
    }

    signal(SIGHUP, sig_handler);
    signal(SIGINT, sig_handler);

    q = beeQ_init(RUN_QUEUE_SIZE,
                  app_recv_setup,
                  app_recv_proc, NULL);
    if (q == NULL) {
        exit(1);
    }

    proto_que = proto_sender_init(q);
    if (proto_que == NULL) {
        exit(1);
    }
    beeQ_send_thread(q, proto_que, app_collector_run);

    beaver_init(g_yaml_file);
    pause();
    fprintf(stderr, "loop exit.");
    beeQ_stop(q);
    beeQ_stop(proto_que);
    return 0;
}
