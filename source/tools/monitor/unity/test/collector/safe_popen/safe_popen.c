//
// Created by 廖肇燕 on 2023/1/28.
//

#include "safe_popen.h"
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/epoll.h>

#include "safe_popen.h"

int safe_fork_read(void (*func)(void*), void* arg, char* buff, int len, int timeout_ms) {
    pid_t pid;
    int fd, pfd[2];

    if (len <= 1) return 0;

    --len;

    if (pipe(pfd) < 0) return -1;

    if ((pid = fork()) < 0) {
        close(pfd[0]);
        close(pfd[1]);
        return -2;
    }

    if (pid == 0) {
        close(pfd[0]);
        fflush(stderr);
        fflush(stdout);

        if (pfd[1] != STDOUT_FILENO) {
            dup2(pfd[1], STDOUT_FILENO);
        }
        if (pfd[1] != STDERR_FILENO) {
            dup2(pfd[1], STDERR_FILENO);
        }

        if (pfd[1] != STDOUT_FILENO && pfd[1] != STDERR_FILENO) close(pfd[1]);

        setpgrp();
        setbuf(stdout, NULL);
        func(arg);
        fflush(stdout);
        _exit(127);
    } else {
        close(pfd[1]);
        fd = pfd[0];
    }

    unsigned int curp = 0;
    int efd = -1, loop = 0;

    if (fcntl(fd, F_SETFD, fcntl(fd, F_GETFD, 0) | O_NONBLOCK) < 0) {
        goto reap_child;
    }

    efd = epoll_create(2);
    if (efd < 0) goto reap_child;

#define max_ep_event 4
    struct epoll_event ev, evs[max_ep_event];

    ev.data.fd = fd;
    ev.events = EPOLLIN;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        goto reap_child;
    }

    struct timeval tv;
    long long start_ms = 0, end_ms = 0;

    tv.tv_sec = tv.tv_usec = 0;
    gettimeofday(&tv, NULL);
    start_ms = end_ms = tv.tv_sec * 1000 + (tv.tv_usec) / 1000;

    do {
        if (end_ms - start_ms > timeout_ms) break;

        int ret = epoll_wait(efd, evs, max_ep_event, timeout_ms);

        tv.tv_sec = tv.tv_usec = 0;
        gettimeofday(&tv, NULL);
        end_ms = tv.tv_sec * 1000 + (tv.tv_usec) / 1000;

        printf("read %d\n", ret);
        if (ret > 0) { // ret == 1
            int sz = read(fd, buff + curp, len - curp);
            printf("read %d\n", sz);
            if (sz < 0) {
                if (errno == EAGAIN || errno == EINTR) continue;
                break;
            }
            if ((curp += sz) >= len || sz == 0) break;
        } else if (0 == ret) {
            break; // timeout
        } else {
            if (errno == EINTR) continue;
            break;
        }
    } while (loop++ < 1024);

    reap_child:
    close(fd);
    buff[curp] = 0;
    kill(-pid, SIGKILL);

    int stat;
    while (waitpid(pid, &stat, 0) < 0) {
        if (errno != EINTR) break;
    }

    if (efd >= 0) close(efd);

    return curp;
}

static void shell_exec_func(void *cmd) {
    execl("/bin/sh", "sh", "-c", cmd, (char *)0);
}

int safe_popen_read(const char *cmd, char *buff, int len, int timeout_ms) {
    return safe_fork_read(shell_exec_func, (void*)(cmd), buff, len, timeout_ms);
}
