//
// Created by 廖肇燕 on 2023/4/10.
//

#ifndef UNITY_VIRTOUT_H
#define UNITY_VIRTOUT_H

#define TASK_COMM_LEN 16

struct data_t {
    int pid;
    int cpu;
    unsigned long us;
    unsigned int stack_id;
    unsigned  long delta;
    char comm[TASK_COMM_LEN];
};

#endif //UNITY_VIRTOUT_H
