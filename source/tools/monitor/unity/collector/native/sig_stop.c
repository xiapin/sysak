//
// Created by 廖肇燕 on 2023/1/28.
//

#include "sig_stop.h"
#include <signal.h>

#include <stdio.h>

static volatile int working = 1;

int plugin_is_working(void) {
    return working;
}

void plugin_stop(void) {
    working = 0;
}

void plugin_thread_stop(pthread_t tid) {
    if (tid > 0) {
        printf("send sig stop to thread %lu\n", tid);
        pthread_kill(tid, SIGQUIT);
        pthread_join(tid, NULL);
    }
}

static void stop_signal_handler(int no) {
    ;
}

static void sig_register(void) {
    struct sigaction action;

    action.sa_handler = stop_signal_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGQUIT, &action, NULL);
}

void plugin_init(void) {
    sig_register();
    working = 1;
}
