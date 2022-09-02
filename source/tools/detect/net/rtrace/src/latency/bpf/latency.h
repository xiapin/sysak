#ifndef __LATENCY_H
#define __LATENCY_H

#include "common.h"

#ifndef u8
typedef unsigned char u8;
#endif

#ifndef u16
typedef unsigned short int u16;
#endif

#ifndef u32
typedef unsigned int u32;
#endif

#ifndef u64
typedef long long unsigned int u64;
#endif


// pid events
enum {
    PID_EVENTS_SCHED_IN = 0,
    PID_EVENTS_SCHED_OUT,
    PID_EVENTS_MAX,
};

// sock event
enum
{
    SOCK_EVENTS_SOCK_DEF_READABLE,
    SOCK_EVENTS_MAX,
};

struct socktime
{
    struct seconds4_ring sockevents[SOCK_EVENTS_MAX];
};

struct pidtime
{
    struct seconds4_ring pidevents[PID_EVENTS_MAX];
};

struct sockmap_value
{
    u64 queue_ts;
    u64 threshold; // in ns
    u32 socktime_idx;
};

struct pidmap_value
{
    u32 pidtime_idx;
};

struct loghist
{
    u32 hist[32];
};

#endif
