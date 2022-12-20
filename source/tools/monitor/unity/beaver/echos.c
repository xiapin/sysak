//
// Created by 廖肇燕 on 2022/12/20.
//

#include "echos.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

#define ECHOS_SIZE 1024

int echos_init(void) {
    return 0;
}

int echos(int fd) {
    size_t ret;
    char stream[ECHOS_SIZE];

    ret = read(fd, stream, ECHOS_SIZE);
    if (ret < 0) {
        goto endRecv;
    }
    ret = write(fd, stream, ret);

    endRecv:
    return ret;
}