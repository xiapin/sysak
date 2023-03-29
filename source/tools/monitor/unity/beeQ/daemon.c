//
// Created by 廖肇燕 on 2023/3/21.
//

#include "daemon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <syslog.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>

#define DAEMON_NAME "daemon"
#define DAEMON_NAME_SIZE sizeof(DAEMON_NAME)
#define DAEMON_PIPE_SIZE 4096

#define TASK_NAME "unity-mon"
#define TASK_NAME_SIZE sizeof(TASK_NAME)

extern char** entry_argv;

static int init_daemon(int fd) {
    pid_t pid;
    char *s;

    pid = fork();
    if (pid < 0) {
        perror("fork error!");
        exit(1);
    }
    else if (pid > 0) {
        exit(0);
    }

    // mask signal.
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    // create new pid group, leave to old pid groups.
    setsid();

    // fork a new child process
    pid = fork();
    if( pid > 0) {
        exit(0);
    }
    else if (pid< 0) {
        exit(1);
    }

    // change process name.
    s = entry_argv[0];
    strcpy(s, DAEMON_NAME);
    s[DAEMON_NAME_SIZE] = '\0';

    // close all no use fd.
    int i;
    for (i = 0; i < NOFILE; i ++){
        if (i != fd) {
            close(i);
        }
    }

    // change work dir. ignore all child signal and file mask
    chdir("/");
    umask(0);
    signal(SIGCHLD, SIG_IGN);
    return 0;
}

static int kill_mon(pid_t mon) {
    int fd;
    char path[64];
    char buffer[64];
    snprintf(path, 64, "/proc/%d/cmdline", mon);

    if (access(path, F_OK) < 0) {
        return errno;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        return errno;
    }
    read(fd, buffer, 64);
    close(fd);
    if (strstr(buffer, TASK_NAME) != NULL) {
        return kill(mon, SIGKILL);
    }
    return 0;
}

static int loop_daemon(pid_t mon, int period, int fd) {
    int ret;
    char buff[DAEMON_PIPE_SIZE];
    fd_set read_fds;  //读文件操作符
    fd_set except_fds;
    struct timeval tv;

    FD_ZERO(&read_fds);
    FD_ZERO(&except_fds);

    init_daemon(fd);
    while (1) {
        FD_SET(fd, &read_fds);
        FD_SET(fd, &except_fds);
        tv.tv_sec = period * 2;
        tv.tv_usec = 0;

        ret = select(fd + 1, &read_fds, NULL, &except_fds, &tv);

        if (ret == 0) {
            syslog(LOG_NOTICE, "daemon: select time out\n");
            kill_mon(mon);
            return 0;
        } else if ( ret == -1) {
            syslog(LOG_NOTICE, "daemon: select return %d\n", errno);
            kill_mon(mon);
            return -errno;
        } else {
            ret = read(fd, buff, DAEMON_PIPE_SIZE);
            if (ret <= 0) {
                kill_mon(mon);
                return 0;
            } else if (strncmp(buff, "stop", 4) == 0) {
                exit(0);
            }
        }
    }
}

static int fork_daemon(int period, int fd0, int fd1) {
    pid_t pid;
    pid_t mon = getpid();

    pid = fork();
    if (pid < 0) {
        perror("fork failed");
        exit(1);
    }
    if (pid == 0) {
        close(fd1);
        loop_daemon(mon, period, fd0);
        exit(0);
    } else {
        wait(NULL);
    }
    close(fd0);
    return fd1;
}

int setup_daemon(int period) {
    int pipefd[2];

    if(pipe(pipefd) < 0) {
        perror("create pipe failed.");
        exit(1);
    }

    return fork_daemon(period, pipefd[0], pipefd[1]);
}
