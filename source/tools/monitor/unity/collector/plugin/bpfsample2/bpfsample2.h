

#ifndef BPF_SAMPLE_H
#define BPF_SAMPLE_H



struct event {
    long ns;
    long cpu;
    int pid;
    char comm[16];
    int size;
};

#ifndef __VMLINUX_H__

#include "../plugin_head.h"

int init(void * arg);
int call(int t, struct unity_lines* lines);
void deinit(void);

#endif

#endif 
