//
// Created by 廖肇燕 on 2023/1/28.
//

#include "thread_poll.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

int set_non_blocking(int fd)
{
    int opts;
    int ret = 0;

    opts = fcntl(fd, F_GETFL);
    if (opts < 0) {
        perror("fcntl(sock,GETFL)");
        ret = -errno;
        goto endGet;
    }

    opts = opts | O_NONBLOCK;
    if (fcntl(fd, F_SETFL, opts) < 0) {
        perror("fcntl(sock,SETFL,opts)");
        ret = -errno;
        goto endSet;
    }
    return ret;

    endSet:
    endGet:
    return ret;
}

static int epoll_init(int fd) {
    int ret;
    int efd;
    struct epoll_event ev;

    ret = set_non_blocking(fd);
    if (ret < 0) {
        efd = ret;
        goto endFcntl;
    }

    efd = epoll_create(32);
    if (efd < 0) {
        ret = -errno;
        perror("epoll_create failed.\n");
        goto endCreate;
    }

    memset(&ev, sizeof(struct epoll_event), 0);
    ev.data.fd = fd;
    ev.events = EPOLLIN;
    ret = epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev);
    if (ret < 0) {
        perror("epoll_ctl failed.\n");
        efd = -errno;
    }
    return efd;

    endCreate:
    endFcntl:
    return efd;
}

static int poll(int efd) {
    int nfds;
    int i;
    struct epoll_event events[4];

    nfds = epoll_wait(efd, events, 4, -1);
    if (nfds < 0) {
        return -errno;
    }

    for (i = 0; i < nfds; i ++) {
        if (events[i].events & EPOLLIN) {
            int fd = events[i].data.fd;
            char line[256];

            read(fd, line, 256);
            printf("read %d, get %s\n", fd, line);
        }
    }
    return 0;
}

static void handle_quit(int no) {
    printf("get signal %d\n", no);
}

int main(void) {
    FILE *file;
    int fd;
    int efd;

    signal(SIGQUIT, handle_quit);

    file = popen("./loop", "r");
    fd = fileno(file);
    printf("open fd: %d\n", fd);

    efd = epoll_init(fd);
    if (efd < 0) {
        exit(1);
    }

    while (poll(efd) == 0);
    close(efd);

    pclose(file);
    return 0;
}