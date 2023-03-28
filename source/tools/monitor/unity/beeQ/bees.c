//
// Created by 廖肇燕 on 2022/12/26.
//

#include "beeQ.h"
#include "apps.h"
#include "beaver.h"
#include "outline.h"
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "clock/ee_clock.h"
#include "postQue/postQue.h"

#define RUN_THREAD_MAX  8
#define RUN_QUEUE_SIZE  32

volatile int sighup_counter = 0;
char *g_yaml_file = NULL;
static pthread_t pid_collector = 0;
static pthread_t pid_outline = 0;

void sig_handler(int num)
{
    printf("receive the signal %d.\n", num);
    switch (num) {
        case SIGHUP:
            sighup_counter ++;
            pthread_kill(pid_collector, SIGUSR1);
            pthread_kill(pid_outline, SIGUSR1);
            break;
        case SIGUSR1:   // to stop
            break;
        default:
            printf("signal %d exit.\n", num);
            exit(1);
    }
}

char ** entry_argv; // for daemon process
extern struct beeQ* proto_sender_init(struct beeQ* pushQ);
int main(int argc, char *argv[]) {
    struct beeQ* q;           //for proto-buf stream
    struct beeQ* proto_que;   //for trans c to proto-buf stream

    entry_argv = argv;
    if (argc > 1) {
        g_yaml_file = argv[1];
    }

    signal(SIGHUP, sig_handler);
    signal(SIGUSR1, sig_handler);
    signal(SIGINT, sig_handler);

    if (calibrate_local_clock() < 0) {
        printf("calibrate_local_clock failed.\n");
        exit(1);
    }

    postQue_init();

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
    pid_collector = beeQ_send_thread(q, proto_que, app_collector_run);
    if (pid_collector == 0) {
        exit(1);
    }

    pid_outline = outline_init(q, g_yaml_file);
    if (pid_outline == 0) {
        exit(1);
    }
    beaver_init(g_yaml_file);

    fprintf(stderr, "loop exit.");
    beeQ_stop(q);
    beeQ_stop(proto_que);
    return 0;
}
