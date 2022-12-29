//
// Created by 廖肇燕 on 2022/12/20.
//

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/syscall.h>
#include <pthread.h>
#include "beaver.h"
#include "echos.h"


#define FD_RATIO    2   // fd_num vs thread
#define gettidv1() syscall(__NR_gettid)

#define unlikely(x)     __builtin_expect((x),0)
#define ASSERT_LOCKS(X) do{ \
if (unlikely(X < 0))        \
{                          \
    perror("hold lock failed."); \
    exit(1);               \
}\
}while(0)

struct beaver_message {
    pthread_mutex_t pc_mutex;
    pthread_cond_t pc_condp, pc_condc;
    int sk_accept;
    int status;     // working is 0, bad is -1;
    int thread;     // thread num
    int fd_num;     // fd pool number max value.
    int fd_count;   // fd pool realtime count
    int* fds;       // fd pool
};

static int beaver_broad_stop(struct beaver_message* pmsg) {
    int ret = 0;
    pmsg->status = -1;

    ret = pthread_cond_broadcast(&pmsg->pc_condc);
    ASSERT_LOCKS(ret);
    ret = pthread_cond_broadcast(&pmsg->pc_condp);
    ASSERT_LOCKS(ret);

    return ret;
}

static int fd_in_beaver(int fd, struct beaver_message* pmsg){
    if (pmsg->status) {  // bad status should exit.
        return 0;
    }

    if (pmsg->fd_count == pmsg->fd_num) {  //fd poll may large
        return 1;
    }

    for (int i = 0; i < pmsg->fd_num; ++i) {
        if (pmsg->fds[i] == fd) {
            return 1;
        }
    }

    return 0;
}

static int add_in_beaver(int fd, struct beaver_message* pmsg){
    for (int i = 0; i < pmsg->fd_num; i ++) {
#ifdef BEAVER_DEBUG
        if (pmsg->fds[i] == fd) {
            fprintf(stderr, "bug: fd %d is already in fds.\n", fd);
            exit(0);
        }
#endif
        if (pmsg->fds[i] == -1) {
            pmsg->fds[i] = fd;
            pmsg->fd_count ++;
            return 0;
        }
    }
#ifdef BEAVER_DEBUG
    fprintf(stderr, "bug: add %d to fds failed..\n\t", fd);
    for (int i = 0; i < pmsg->fd_num; i ++) {
        fprintf(stderr, "%d ", pmsg->fds[i]);
    }
    fprintf(stderr, "\n");
    exit(0);
#endif
    return 1;
}

static int del_in_beaver(int fd, struct beaver_message* pmsg){
    int ret;
    ret = pthread_mutex_lock(&pmsg->pc_mutex);
    ASSERT_LOCKS(ret);
    for (int i = 0; i < pmsg->fd_num; ++i) {
        if (pmsg->fds[i] == fd) {
            pmsg->fds[i] = -1;
            pmsg->fd_count --;
            ret = pthread_cond_signal(&pmsg->pc_condp);
            ASSERT_LOCKS(ret);
            ret = pthread_mutex_unlock(&pmsg->pc_mutex);
            ASSERT_LOCKS(ret);
            return 0;
        }
    }
#ifdef BEAVER_DEBUG
    fprintf(stderr, "bug: fd %d is not in fds.\n", fd);
    exit(0);
#endif
    ret = pthread_mutex_unlock(&pmsg->pc_mutex);
    ASSERT_LOCKS(ret);
    return 1;
}

static void * beaver_threads(void * arg) {
    int ret;
    int fd;
    int tid = (int)gettidv1();
    struct beaver_message * pmsg = (struct beaver_message *)arg;
    lua_State *L;

    // init is here.
    L = echos_init(tid);
    if (L == NULL) {
        exit(1);
    }

    while (1) {
        ret = pthread_mutex_lock(&pmsg->pc_mutex);
        ASSERT_LOCKS(ret);

        while (pmsg->sk_accept == 0 || pmsg->status != 0){
            ret = pthread_cond_wait(&pmsg->pc_condc, &pmsg->pc_mutex);
            ASSERT_LOCKS(ret);
        }
        if (pmsg->status != 0) {
            goto endStatus;
        }
        fd = pmsg->sk_accept;
        pmsg->sk_accept = 0;
        ret = pthread_mutex_unlock(&pmsg->pc_mutex);
        ASSERT_LOCKS(ret);

        //work here
        ret = echos(L, fd);
        close(fd);
        del_in_beaver(fd, pmsg);

        if (ret) {
            goto endThread;
        }
    }
    endStatus:
    ret = pthread_mutex_unlock(&pmsg->pc_mutex);
    ASSERT_LOCKS(ret);
    endThread:
    lua_close(L);
    beaver_broad_stop(pmsg);
    pthread_exit(NULL);
}

static int beaver_setup_message(struct beaver_message* pmsg) {
    int ret;
    int fret;

    pmsg->sk_accept = 0;
    pmsg->status = 0;

    ret = pthread_mutex_init(&pmsg->pc_mutex, NULL);
    if (ret < 0) {
        perror("pc_mutex create failed.");
        goto endMutex;
    }
    ret = pthread_cond_init(&pmsg->pc_condp, NULL);
    if (ret < 0) {
        perror("pc_condp create failed.");
        goto endCondp;
    }
    ret = pthread_cond_init(&pmsg->pc_condc, NULL);
    if (ret < 0) {
        perror("pc_condc create failed.");
        goto endCondc;
    }

    return ret;
    endCondc:
    fret = pthread_cond_destroy(&pmsg->pc_condp);
    if (fret < 0){
        perror("destroy condp faild.");
        exit(1);
    }
    endCondp:
    fret = pthread_mutex_destroy(&pmsg->pc_mutex);
    if (fret < 0){
        perror("destroy pc_mutex faild.");
        exit(1);
    }
    endMutex:
    return ret;
}

static void beaver_destroy_message(struct beaver_message* pmsg) {
    int ret;

    ret = pthread_cond_destroy(&pmsg->pc_condc);
    if (ret < 0){
        perror("destroy condc faild.");
        exit(1);
    }
    ret = pthread_cond_destroy(&pmsg->pc_condp);
    if (ret < 0){
        perror("destroy condp faild.");
        exit(1);
    }
    ret = pthread_mutex_destroy(&pmsg->pc_mutex);
    if (ret < 0){
        perror("destroy pc_mutex faild.");
        exit(1);
    }
}

static int beaver_threads_start(int thread, pthread_t ** tid_arr, struct beaver_message* pmsg) {
    int ret = 0;
    pthread_t *tids;

    tids = (pthread_t *) malloc(thread * sizeof (pthread_t));
    if (tids == NULL) {
        perror("beaver thread malloc.");
        ret = -ENOMEM;
        goto endMalloc;
    }
    for (int i = 0; i < thread; i ++) {
        tids[i] = 0;
    }
    *tid_arr = tids;

    ret = beaver_setup_message(pmsg);
    if (ret < 0) {
        goto endMessage;
    }

    for (int i = 0; i < thread; i ++) {
        ret = pthread_create(&tids[i], NULL, beaver_threads, pmsg);
        ASSERT_LOCKS(ret);
    }

    return ret;
    endMessage:
    free(tids);
    endMalloc:
    return ret;
}

static int beaver_thread_stop(int thread, pthread_t ** tid_arr, struct beaver_message* pmsg) {
    int ret = 0;
    pthread_t * tids = *tid_arr;

    beaver_broad_stop(pmsg);
    for (int i = 0; i < thread; i ++) {
        if (tids[i] > 0) {
            ret = pthread_join(tids[i], NULL);
            ASSERT_LOCKS(ret);
        }
    }
    free(tids);
    *tid_arr = NULL;
    return 0;
}

static int beaver_accept(int sk_listen, struct beaver_message* pmsg) {
    struct sockaddr_in cli_addr;
    socklen_t len = sizeof(cli_addr);
    int sk_accept;
    int ret;

    ret = pthread_mutex_lock(&pmsg->pc_mutex);
    ASSERT_LOCKS(ret);
    pmsg->fds = malloc(sizeof (int) * pmsg->fd_num);
    if (pmsg->fds == NULL) {
        ret = -ENOMEM;
        goto endMem;
    }
    for (int i = 0; i < pmsg->fd_num; i ++) {
        pmsg->fds[i] = -1;
    }
    ret = pthread_mutex_unlock(&pmsg->pc_mutex);
    ASSERT_LOCKS(ret);

    while (1) {
        sk_accept = accept(sk_listen, (struct sockaddr *)&cli_addr, &len);
        if (sk_accept < 0) {
            perror("accept failed.");
            ret = sk_accept;
            goto endAccept;
        }

        ret = pthread_mutex_lock(&pmsg->pc_mutex);
        ASSERT_LOCKS(ret);
        while (fd_in_beaver(sk_accept, pmsg)){   // new fd may re
            ret = pthread_cond_wait(&pmsg->pc_condp, &pmsg->pc_mutex);
            ASSERT_LOCKS(ret);
        }
        if (pmsg->status) {
            ret = pthread_mutex_unlock(&pmsg->pc_mutex);
            ASSERT_LOCKS(ret);
            goto endStatus;
        }
        add_in_beaver(sk_accept, pmsg);

        pmsg->sk_accept = sk_accept;
        ret = pthread_cond_signal(&pmsg->pc_condc);
        ASSERT_LOCKS(ret);

        ret = pthread_mutex_unlock(&pmsg->pc_mutex);
        ASSERT_LOCKS(ret);
    }
    endStatus:
    endAccept:
    free(pmsg->fds);
    endMem:
    return ret;
}

int beaver_init(int port, int thread) {
    int ret;
    int sk_listen;
    int sockopt = 1;
    struct sockaddr_in srvaddr;
    pthread_t * tids;
    struct beaver_message msg;

    msg.thread = thread;
    msg.fd_num = thread * FD_RATIO;
    msg.fd_count = 0;

    sk_listen = socket(AF_INET, SOCK_STREAM, 0);
    if (sk_listen < 0) {
        perror("create socket failed.");
        ret = -ENOENT;
        goto endSocket;
    }

    srvaddr.sin_family = AF_INET;
    srvaddr.sin_port = htons(port);
    srvaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    ret = setsockopt(sk_listen, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(int));
    if (ret < 0) {
        perror("setsockopt socket failed.");
        goto endSetoption;
    }

    ret = bind(sk_listen, (struct sockaddr *)&srvaddr, sizeof(srvaddr));
    if (ret < 0) {
        perror("bind socket failed.");
        goto endBind;
    }

    ret = listen(sk_listen, thread);
    if (ret < 0) {
        perror("listen socket failed.");
        goto endListen;
    }

    ret = beaver_threads_start(thread, &tids, &msg);
    if (ret < 0) {
        goto endThread;
    }

    printf("listen %d for %d threads.\n", port, thread);
    beaver_accept(sk_listen, &msg);
    beaver_thread_stop(thread, &tids, &msg);
    return ret;
    endThread:
    endListen:
    endBind:
    endSetoption:
    close(sk_listen);
    endSocket:
    return ret;
}
