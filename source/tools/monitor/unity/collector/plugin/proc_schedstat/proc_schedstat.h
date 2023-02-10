//
// Created by 廖肇燕 on 2022/12/30.
//

#ifndef UNITY_PROC_SCHEDSTAT_H
#define UNITY_PROC_SCHEDSTAT_H

#include "../plugin_head.h"

int init(void * arg);
int call(int t, struct unity_lines* lines);
void deinit(void);

#endif //UNITY_PROC_SCHEDSTAT_H
