#ifndef __USER_SLOW_H
#define __USER_SLOW_H

struct filter
{
    unsigned long long threshold;
};

struct sched_event
{
    int prev_pid;
    int next_pid;
    unsigned char prev_comm[16];
    unsigned char next_comm[16];
    unsigned long long ts;
};

struct slow_event
{
    unsigned long long krcv_ts;
    unsigned long long urcv_ts;

    unsigned int saddr;
    unsigned int daddr;
    unsigned short sport;
    unsigned short dport;

    struct sched_event sched;
};



#endif