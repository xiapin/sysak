#ifndef __CONNECTLATENCY_H
#define __CONNECTLATENCY_H


#include "common.h"

#define MAX_EVENT_NUM 10

enum
{
    TCP_CONNECT,
    TCP_CONNECT_RET,
    TCP_RCV_SYNACK,

};

struct sockmap_val
{
    struct addr_pair ap;
    u8 comm[16];
    u32 pid;
    u32 curidx;
    int ret;
    struct
    {
        u64 ts;
        u32 event;
    } tss[MAX_EVENT_NUM];
};

#endif
