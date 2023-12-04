#ifndef __QUEUE_SLOW_H
#define __QUEUE_SLOW_H

struct filter
{
    unsigned long long threshold;
    unsigned int protocol;
};


struct queue_slow
{
    unsigned int saddr;
    unsigned int daddr;
    unsigned short sport;
    unsigned short dport;
    unsigned int protocol;
    unsigned long long latency;
};

#endif 