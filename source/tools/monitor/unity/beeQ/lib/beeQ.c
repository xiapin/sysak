//
// Created by 廖肇燕 on 2022/12/7.
//

#include "../beeQ.h"
#include <errno.h>
#include <unistd.h>

#define USE_BEEQ_DEBUG
#ifdef USE_BEEQ_DEBUG
#define BEEQ_DEBUG(...)\
        do{\
            fprintf(stderr,"-----BEEQ DEBUG-----\n");\
            fprintf(stderr,"%s %s\n",__TIME__,__DATE__);\
            fprintf(stderr,"%s:%d:%s():",__FILE__,__LINE__,__func__);\
            fprintf(stderr,__VA_ARGS__);\
        }while(0)
#else
#define BEEQ_DEBUG(...)\
do{}while(0)
#endif

#define LOOP_QUEUE_MAX 32

struct beeMsg {
    struct beeQ *q;
    void *sarg;
    int (*cb)(struct beeQ *q, void* arg);
};

static int isfull(struct beeQ* q) {
    return (q->send + 1) % q->size == q->recv;
}

static int isempty(struct beeQ* q) {
    return q->send == q->recv;
}

static int beeQ_register(struct beeQ *q) {
    int i;
    pthread_t tid = pthread_self();

    pthread_mutex_lock(&q->mtx);
    if (q->stop || q->tid_count >= BEEQ_TIDS) {
        pthread_mutex_unlock(&q->mtx);
        return -ENOENT;
    }

    for (i = 0; i < BEEQ_TIDS; i ++) {
        if (q->tids[i] == 0) {
            q->tids[i] = tid;
            break;
        }
    }

    q->tid_count ++;
    pthread_mutex_unlock(&q->mtx);
    return 0;
}

static int beeQ_thread_exit(struct beeQ *q) {
    int i;
    pthread_t tid = pthread_self();

    pthread_mutex_lock(&q->mtx);
    if (q->tid_count == 0) {
        pthread_mutex_unlock(&q->mtx);
        return 0;
    }
    for (i = 0; i < BEEQ_TIDS; i ++) {
        if (q->tids[i] == tid) {
            q->tids[i] = 0;
            break;
        }
    }
    pthread_mutex_unlock(&q->mtx);
    return 0;
}

static int beeQ_join(struct beeQ *q) {
    int i;

    pthread_mutex_lock(&q->mtx);
    for (i = 0; i < BEEQ_TIDS; i ++) {
        if (q->tids[i] != 0) {
            pthread_mutex_unlock(&q->mtx);
            pthread_join(q->tids[i], NULL);
            pthread_mutex_lock(&q->mtx);
        }
    }
    pthread_mutex_unlock(&q->mtx);
    return 0;
}

static void * beeQ_proc(void * arg) {
    int res;
    struct beeQ* q = (struct beeQ*)arg;

    if (beeQ_register(q)) {
        return NULL;
    }
    if (q->init != NULL) {
        res = q->init(q);
        if (res < 0) {
            goto endProc;
        }
    }

    pthread_mutex_lock(&q->mtx);
    pthread_cond_signal(&q->cond);
    while (q->stop == 0) {
        pthread_cond_wait(&q->cond, &q->mtx);
        while (!isempty(q)) {
            void* msg;

            q->recv = (q->recv + 1) % q->size;
            msg = q->msgs[q->recv];
            q->msgs[q->recv] = NULL;

            pthread_mutex_unlock(&q->mtx);  // Processing messages can be very time consuming
            q->cb(msg, q);     // the call back function should
            pthread_mutex_lock(&q->mtx);
        }
    }
    pthread_mutex_unlock(&q->mtx);

    endProc:
    beeQ_thread_exit(q);
    return NULL;
}

struct beeQ* beeQ_init(int size,
        int (*init)(struct beeQ* q),
        int (*cb)(void *msg, struct beeQ* q),
        void *arg) {
    int res;
    struct beeQ* q;
    pthread_t tid;
    int i;

    q = (struct beeQ*) malloc(sizeof(struct beeQ));
    if (q == NULL) {
        errno = ENOMEM;
        goto failStruct;
    }

    pthread_mutex_init(&q->mtx, NULL);
    pthread_cond_init(&q->cond, NULL);
    q->stop = 0;

    q->recv = 0;
    q->send = 0;
    q->size = size;

    q->msgs = (void **) malloc(size * sizeof(void *));
    if (q->msgs == NULL) {
        errno = ENOMEM;
        goto failMsg;
    }
    for (i = 0; i < size; i ++) {
        q->msgs[i] = NULL;
    }
    q->init = init;
    q->cb   = cb;
    q->qarg = arg;

    q->tid_count = 0;
    for (i = 0; i < BEEQ_TIDS; i ++) {
        q->tids[i] = 0;
    }

    res = pthread_create(&tid, NULL, beeQ_proc, (void *)q);
    if (res == -1) {
        errno = ENOENT;
        goto failThread;
    }

    // confirm receiver thread is wait for queue.
    pthread_mutex_lock(&q->mtx);
    pthread_cond_wait(&q->cond, &q->mtx);
    pthread_mutex_unlock(&q->mtx);
    return q;

    failThread:
    free(q->msgs);
    failMsg:
    free(q);
    failStruct:
    return NULL;
}

int beeQ_stop(struct beeQ *q) {

    pthread_mutex_lock(&q->mtx);
    q->stop = 1;
    if (isempty(q)) {
        pthread_cond_signal(&q->cond);
    }
    pthread_mutex_unlock(&q->mtx);

    beeQ_join(q);

    pthread_mutex_destroy(&q->mtx);
    pthread_cond_destroy(&q->cond);

    while (!isempty(q)) {   // clear msg queue;
        void* msg;

        q->recv = (q->recv + 1) % q->size;
        msg = q->msgs[q->recv];
        q->msgs[q->recv] = NULL;

        free(msg);
    }

    free(q->msgs);
    q->msgs = NULL;
    free(q);
    return 0;
}

int beeQ_send(struct beeQ *q, void *msg) {
    int loop = 0;

    pthread_mutex_lock(&q->mtx);
    if (q->stop) {
        pthread_mutex_unlock(&q->mtx);
        free(msg);
        return -1;
    }

    while (isfull(q)) {  // full
        pthread_mutex_unlock(&q->mtx);
        usleep(30000);
        loop ++;
        if (loop < LOOP_QUEUE_MAX) {
            pthread_mutex_lock(&q->mtx);    //continue.
        } else {
            fprintf(stderr, "message que is full now.\n");
            free(msg);
            return 0;
        }
    }

    if (isempty(q)) {
        pthread_cond_signal(&q->cond);  // need to wakeup.
    }
    q->send = (q->send + 1) % q->size;
    q->msgs[q->send] = msg;
    pthread_mutex_unlock(&q->mtx);
    return 0;
}

static void * beeQ_send_run(void * args) {
    struct beeMsg* msg = (struct beeMsg*)args;
    struct beeQ *q = msg->q;
    void *sarg = msg->sarg;
    int (*cb)(struct beeQ *q, void* sarg) = msg->cb;

    free(args);

    if (beeQ_register(q)) {
        return NULL;
    }

//    BEEQ_DEBUG("SEND QUEUE IS WORKING.\n");
    cb(q, sarg);
    beeQ_thread_exit(q);
    return NULL;
}

pthread_t beeQ_send_thread(struct beeQ *q, void *sarg, int (*cb)(struct beeQ *q, void* arg)) {
    pthread_t tid;
    int res;
    struct beeMsg* msg;

    if (q->stop) {
        return -ENOENT;
    }

    msg = malloc(sizeof(struct beeMsg));
    if (msg == NULL) {
        res = -ENOMEM;
        goto failMalloc;
    }

    msg->q = q;
    msg->sarg = sarg;
    msg->cb = cb;

    res = pthread_create(&tid, NULL, beeQ_send_run, (void *)msg);
    if (res == -1) {
        res = -ENOENT;
        goto failThread;
    }
    return tid;

    failThread:
    free(msg);
    failMalloc:
    return res;
}
