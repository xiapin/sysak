//
// Created by 廖肇燕 on 2023/1/28.
//
#define _GNU_SOURCE
#include "kmsg.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <sys/prctl.h>

#define KMSG_LINE 8192

static volatile pthread_t kmsg_thread_id = 0;

static int kmsg_thread_func(struct beeQ* q, void * arg);
int init(void * arg) {
    struct beeQ* q = (struct beeQ *)arg;
    kmsg_thread_id = beeQ_send_thread(q, NULL, kmsg_thread_func);
    printf("start kmsg_thread_id: %lu\n", kmsg_thread_id);
    return 0;
}

static int kmsg_set_block(int fd) {
    int ret;
    int flags = fcntl(fd, F_GETFL);
    flags &= ~O_NONBLOCK;
    ret = fcntl(fd, F_SETFL, flags);
    return ret;
}

static int kmsg_thread_func(struct beeQ* q, void * arg) {
    int fd;
    int ret;
    char buff[KMSG_LINE];
    prctl(PR_SET_NAME, (unsigned long)"kmsg collector");

    fd = open("/dev/kmsg", O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        goto endOpen;
    }
    ret = lseek(fd, 0, SEEK_DATA);
    if (ret < 0) {
        perror("kmsg seek error");
        goto endLseek;
    }

    ret = 1;   // strip old message.
    while (ret > 0) {
        ret = read(fd, buff, KMSG_LINE - 1);
        if (ret < 0) {
            if (errno == EAGAIN) {
                break;
            }
            perror("kmsg read failed.");
            goto endRead;
        }
    }

    ret = kmsg_set_block(fd);
    if (ret < 0) {
        perror("kmsg set block failed.");
        goto endBlock;
    }

    while (plugin_is_working()) {
        struct unity_line* line;
        struct unity_lines * lines = unity_new_lines();

        ret = read(fd, buff, KMSG_LINE - 1);
        if (ret < 0) {
            if (errno == EINTR) {
                break;
            }
            printf("errno: %d\n", errno);
            perror("kmsg read2 failed.");
            goto endRead;
        }

        unity_alloc_lines(lines, 1);
        line = unity_get_line(lines, 0);
        unity_set_table(line, "kmsg");
        unity_set_log(line, "log", buff);
        beeQ_send(q, lines);
    }

    close(fd);
    return 0;
    endBlock:
    endRead:
    endLseek:
    close(fd);
    endOpen:
    return 1;
}

int call(int t, struct unity_lines* lines) {
    return 0;
}

void deinit(void) {
    plugin_thread_stop(kmsg_thread_id);
    printf("thread plugin uninstall\n");
}