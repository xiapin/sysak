//
// Created by 廖肇燕 on 2022/12/26.
//

#ifndef UNITY_APPS_H
#define UNITY_APPS_H

#include <lauxlib.h>
#include <lualib.h>

struct beeMsg {
    int size;
    char body[];
};

int app_recv_setup(struct beeQ* q);
int app_recv_proc(void* msg, struct beeQ* q);
int app_collector_run(struct beeQ* q, void* arg);

#endif //UNITY_APPS_H
