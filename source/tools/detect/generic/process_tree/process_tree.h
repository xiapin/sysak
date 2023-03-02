#ifndef __PROCESS_TREE_H
#define __PROCESS_TREE_H

#define MAX_DEPTH 10
#define TASK_COMM_LEN 16
#define MAX_ARR_LEN 255

struct process_tree_event
{
    int type; // 0 (fork), 1 (exec)
    int c_pid;
    int p_pid;
    char comm[TASK_COMM_LEN];
    char args[MAX_ARR_LEN];
};

struct args
{
    int pid;
    // char comm[TASK_COMM_LEN];
};

#endif /* __PROCESS_TREE_H */