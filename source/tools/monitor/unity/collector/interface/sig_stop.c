//
// Created by 廖肇燕 on 2023/1/28.
//

#include "sig_stop.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/resource.h>
#include "fastKsym.h"

static volatile int working = 1;

int plugin_is_working(void) {
    return working;
}

void plugin_stop(void) {
    working = 0;
}

void plugin_thread_stop(pthread_t tid) {
    if (tid > 0) {
        printf("send sig user2 to thread %lu\n", tid);
        pthread_kill(tid, SIGUSR2);
        pthread_join(tid, NULL);
    }
}

static void bump_memlock_rlimit1(void)
{
    struct rlimit rlim_new = {
            .rlim_cur	= RLIM_INFINITY,
            .rlim_max	= RLIM_INFINITY,
    };

    if (setrlimit(RLIMIT_MEMLOCK, &rlim_new)) {
        fprintf(stderr, "Failed to increase RLIMIT_MEMLOCK limit!\n");
        exit(1);
    }
}

void plugin_init(void) {
    bump_memlock_rlimit1();
    ksym_setup(1);
    working = 1;
}

void plugin_deinit(void) {
    ksym_free();
}
