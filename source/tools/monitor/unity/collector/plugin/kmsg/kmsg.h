//
// Created by 廖肇燕 on 2023/1/28.
//

#ifndef UNITY_KMSG_H
#define UNITY_KMSG_H

#include "../plugin_head.h"

int init(void * arg);
int call(int t, struct unity_lines* lines);
void deinit(void);

#endif //UNITY_KMSG_H
