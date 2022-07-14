
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

#define MAX_LATENCY_MS 1024
#define MAX_LATENCY_SLOTS (MAX_LATENCY_MS >> 3)

enum {
    LATENCY_EVENT = 0,
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
