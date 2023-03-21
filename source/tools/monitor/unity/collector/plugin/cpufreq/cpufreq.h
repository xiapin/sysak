//
// Created by muya.
//

#ifndef UNITY_CPUFREQ_H
#define UNITY_CPUFREQ_H

#include "../plugin_head.h"

int init(void * arg);
int call(int t, struct unity_lines* lines);
void deinit(void);

#endif //UNITY_CPUFREQ_H
