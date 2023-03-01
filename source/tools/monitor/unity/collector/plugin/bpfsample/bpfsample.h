

#ifndef BPF_SAMPLE_H
#define BPF_SAMPLE_H

#ifndef __VMLINUX_H__

#include "../plugin_head.h"

int init(void *arg);
int call(int t, struct unity_lines *lines);
void deinit(void);

#endif

#endif
