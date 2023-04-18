//
// Created by 廖肇燕 on 2023/4/18.
//

#define _GNU_SOURCE

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#define KMSG_LINE 8192

static int kmsg_set_block(int fd) {
    int ret;
    int flags = fcntl(fd, F_GETFL);
    flags &= ~O_NONBLOCK;
    ret = fcntl(fd, F_SETFL, flags);
    return ret;
}

int kmsg_thread_func(void) {
    int fd;
    int ret;
    char buff[KMSG_LINE];


    fd = open("/dev/kmsg", O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        goto endOpen;
    }/*
    ret = lseek(fd, 0, SEEK_DATA);
    if (ret < 0) {
        perror("kmsg seek error");
        goto endLseek;
    }*/

    ret = 1;   // strip old message.
    while (ret > 0) {
        ret = read(fd, buff, KMSG_LINE - 1);
        buff[ret] = '\0';
        printf("%s\n", buff);
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

    while (1) {

        ret = read(fd, buff, KMSG_LINE - 1);
        if (ret < 0) {
            if (errno == EINTR) {
                break;
            }
            perror("kmsg read2 failed.");
            goto endRead;
        }
        buff[ret -1] = '\0';

        printf("read: %s\n", buff);
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

int main(void) {
    kmsg_thread_func();
    return 0;
}