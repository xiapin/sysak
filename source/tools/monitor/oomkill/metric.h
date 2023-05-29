#ifndef __MISC__
#define __MISC__
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>

struct cpu_util {
    float user;
    float nice;
    float system;
    float system_avg10;
    float system_avg30;
    float system_avg60;
    float idle;
    float iowait;
    float iowait_avg10;
    float iowait_avg30;
    float iowait_avg60;
    float irq;
    float softirq;
    float steal;
    float guest;
    float guest_nice;
};

struct cpu_stat {
    long user;
    long nice;
    long system;
    long idle;
    long iowait;
    long irq;
    long softirq;
    long steal;
    long guest;
    long guest_nice;
};

#endif
