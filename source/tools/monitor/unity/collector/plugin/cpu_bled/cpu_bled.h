//
// Created by 廖肇燕 on 2023/3/21.
//

#ifndef UNITY_CPU_BLED_H
#define UNITY_CPU_BLED_H

#include "../plugin_head.h"

int init(void * arg);
int call(int t, struct unity_lines* lines);
void deinit(void);

#endif //UNITY_CPU_BLED_H
