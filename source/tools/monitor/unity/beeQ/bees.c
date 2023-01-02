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

void sig_handler(int num)
{
    printf("receive the signal %d.\n", num);
    if (num == SIGHUP) {
        sighup_counter ++;
    } else {
        exit(1);
    }
}

static int beeQ_collectors(struct beeQ* q) {
    beeQ_send_thread(q, NULL, app_collector_run);
}

int main(int argc, char *argv[]) {
    lua_State *L;
    lua_State **pL;
    struct beeQ* q;

    signal(SIGHUP, sig_handler);
    signal(SIGINT, sig_handler);

    L = app_recv_init();
    if (L == NULL) {
        exit(1);
    }

    pL = &L;
    q = beeQ_init(RUN_QUEUE_SIZE, app_recv_proc, (void *)pL);
    if (q == NULL) {
        exit(1);
    }
    beeQ_send_thread(q, NULL, app_collector_run);

    beaver_init(8400, 3);
    pause();
    fprintf(stderr, "test exit.");
    beeQ_stop(q);
    return 0;
}
