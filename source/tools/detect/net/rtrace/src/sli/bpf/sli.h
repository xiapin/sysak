
#ifndef __SLI_H
#define __SLI_H

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

#define MAX_LATENCY_MS 1000
#define MAX_LATENCY_SLOTS (MAX_LATENCY_MS)

enum {
    LATENCY_EVENT = 0,
    USR_LATENCY_EVENT,
};

struct addr_pair
{
    u32 saddr;
    u32 daddr;
    u16 sport;
    u16 dport;
};

struct latency_hist
{
    u32 overflow;
    //max latency slot by user setting
    u32 threshold;
    u32 latency[MAX_LATENCY_SLOTS];
};

struct latency_event
{
    struct addr_pair ap;
    u32 latency;
};

struct event
{
    u8 event_type;
    union
    {
        struct latency_event le;
    };
};

#endif
