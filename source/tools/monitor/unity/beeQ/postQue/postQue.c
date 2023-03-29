//
// Created by 廖肇燕 on 2023/3/24.
//

#include "postQue.h"
#include <string.h>
#include <errno.h>

#define UNITY_POSTQUE_NUM 4
#define UNITY_POSTQUE_MSG_SIZE 128

// post que for out post
struct unity_postQue {
    int num;
    pthread_mutex_t mtx;
    char msgs[UNITY_POSTQUE_NUM][UNITY_POSTQUE_MSG_SIZE];
};

static struct unity_postQue que;

int postQue_pull(char *msg) {
    int ret;
    int i;

    pthread_mutex_lock(&que.mtx);
    ret = que.num;
    for (i = 0; i < ret; i ++) {
        strcat(msg, que.msgs[i]);
        strcat(msg, "\n");
    }
    que.num = 0;
    pthread_mutex_unlock(&que.mtx);
    if (ret) {  // strip last \n
        int len = strlen(msg);
        msg[len - 1]  = '\0';
    }
    return ret;
}

// post a message
int postQue_post(const char *msg) {
    int ret = 0;
    int len = strlen(msg);
    if (len >= UNITY_POSTQUE_MSG_SIZE) {
        return -EINVAL;
    }

    pthread_mutex_lock(&que.mtx);
    if (que.num < UNITY_POSTQUE_NUM) {
        strcpy(que.msgs[que.num], msg);
        que.num ++;
    } else {
        ret = -EAGAIN;
    }
    pthread_mutex_unlock(&que.mtx);
    return ret;
}

int postQue_init() {
    memset(&que, 0, sizeof (struct unity_postQue));
    pthread_mutex_init(&que.mtx, NULL);
    return 0;
}
