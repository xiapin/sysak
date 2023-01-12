//
// Created by 廖肇燕 on 2022/12/7.
//

#ifndef TINYINFO_BEEQ_H
#define TINYINFO_BEEQ_H
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define BEEQ_TIDS   32

struct beeQ {
    pthread_mutex_t mtx;
    pthread_cond_t cond;
    int stop;

    int send;
    int recv;
    int size;

    void **msgs;
    void *arg;
    int (*cb)(void *msg, void *arg);  // callback for message.

    int tid_count;
    pthread_t tids[BEEQ_TIDS];
};

int beeQ_send(struct beeQ *q, void *msg);
pthread_t beeQ_send_thread(struct beeQ *q, void *arg, int (*cb)(struct beeQ *q, void* arg));
struct beeQ* beeQ_init(int size, int (*cb)(void *msg, void* arg), void *arg);
int beeQ_stop(struct beeQ *q);

#endif //TINYINFO_BEEQ_H
