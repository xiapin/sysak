#ifndef TASKTOP_COMMON_H
#define TASKTOP_COMMON_H

struct proc_fork_info_t {
    pid_t pid;
    pid_t ppid;
    u_int64_t fork;
    char comm[16];
};
#endif