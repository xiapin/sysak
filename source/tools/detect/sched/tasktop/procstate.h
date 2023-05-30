#ifndef __PROCSTATE_H
#define __PROCSTATE_H

#include "tasktop.h"

#define LOADAVG_PATH "/proc/loadavg"
#define SCHED_DEBUG_PATH "/proc/sched_debug"

struct proc_state_t {
    int nr_runnable;
    int nr_unint;
};

struct loadavg_t {
    float load1;
    float load5;
    float load15;
    int nr_running;
    int nr_threads;
    long new_pid;
};

int runnable_proc(struct sys_record_t *sys);
int unint_proc(struct sys_record_t *sys);

#endif