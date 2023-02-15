//
// Created by 廖肇燕 on 2023/2/7.
//

#ifndef UNITY_VIRTOUT_H
#define UNITY_VIRTOUT_H

#define TASK_COMM_LEN 16
#define CON_NAME_LEN 80
struct data_t {
    int pid;
    u32 stack_id;
    u64 delta;
    char comm[TASK_COMM_LEN];
    char con[CON_NAME_LEN];
};

#ifndef __VMLINUX_H__

#include "../plugin_head.h"
#include "../bpf_head.h"

#endif

#endif //UNITY_VIRTOUT_H
