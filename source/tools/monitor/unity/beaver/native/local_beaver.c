//
// Created by 廖肇燕 on 2023/2/14.
//

#include "local_beaver.h"
#include <sys/epoll.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int setsockopt_AP(int fd){
    int opt =1;
    int r = setsockopt(fd, SOL_SOCKET,SO_REUSEPORT,(char*)&opt,sizeof(int));
//    SO_REUSEPORT
    if(r<0){
        perror("set sock opt");
    }
    return r;
}

static int socket_non_blocking(int sfd)
{
    int flags, res;

    flags = fcntl(sfd, F_GETFL);
    if (flags < 0) {
        perror("error : cannot get socket flags!\n");
        return -errno;
    }

    flags |= O_NONBLOCK;
    res    = fcntl(sfd, F_SETFL, flags);
    if (res < 0) {
        perror("error : cannot set socket flags!\n");
        return -errno;
    }

    return 0;
}

static int epoll_add(int efd, int fd) {
    struct epoll_event event;
    int res;

    event.events  = EPOLLIN;
    event.data.fd = fd;

    res = epoll_ctl(efd, EPOLL_CTL_ADD, fd, &event);
    if (res < 0) {
        perror("error : can not add event to epoll!\n");
        return -errno;
    }
    return res;
}

static int epoll_del(int efd, int fd) {
    int res;

    res = epoll_ctl(efd, EPOLL_CTL_DEL, fd, NULL);
    if (res < 0) {
        perror("error : can not del event to epoll!\n");
        return -errno;
    }
    return res;
}

int init(int listen_fd) {
    int efd;
    int res;

    efd = epoll_create(NATIVE_EVENT_MAX);
    if (efd < 0) {
        perror("error : cannot create epoll!\n");
        exit(1);
    }

    res = epoll_add(efd, listen_fd);
    if (res < 0) {
        goto end_epoll_add;
    }
    return efd;

    end_epoll_add:
    return -errno;
}

int add_fd(int efd, int fd) {
    int res;

    res = socket_non_blocking(fd);
    if (res < 0) {
        goto end_socket_non_blocking;
    }

    res = epoll_add(efd, fd);
    if (res < 0) {
        goto end_epoll_add;
    }
    return res;

    end_socket_non_blocking:
    end_epoll_add:
    return res;
}

int mod_fd(int efd, int fd, int wr) {
    struct epoll_event event;
    int res;

    event.events  =  wr ? EPOLLIN | EPOLLOUT : EPOLLIN;
    event.data.fd = fd;

    res = epoll_ctl(efd, EPOLL_CTL_MOD, fd, &event);
    if (res < 0) {
        perror("error : can not add event to epoll!\n");
        return -errno;
    }
    return res;
}

int del_fd(int efd, int fd) {
    int res;
    res = epoll_del(efd, fd);

    close(fd);
    return res;
}

int poll_fds(int efd, int tmo, native_events_t* nes) {
    struct epoll_event events[NATIVE_EVENT_MAX];
    int i, res;

    res = epoll_wait(efd, events, NATIVE_EVENT_MAX, tmo * 1000);
    if (res < 0) {
        perror("error : epoll failed!\n");
        return -errno;
    }
    nes->num = res;
    for (i = 0; i < res; i ++) {
        nes->evs[i].fd = events[i].data.fd;

        if ( (events[i].events & EPOLLERR) ||
             (events[i].events & EPOLLHUP) ) {
            nes->evs[i].ev_close = 1;
        }
        if (events[i].events & EPOLLIN) {
            nes->evs[i].ev_in = 1;
        }
        if (events[i].events & EPOLLOUT) {
            nes->evs[i].ev_out = 1;
        }
    }
    return 0;
}

void deinit(int efd) {
    close(efd);
}
