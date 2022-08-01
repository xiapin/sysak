
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
    APP_LATENCY_EVENT,
    DROP_EVENT,
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
    u32 pid;
    u32 latency;
    u8 comm[16];
    struct addr_pair ap;
};

struct drop_event
{
    u32 pid;
    u8 comm[16];
    int stackid;
    u16 protocol;
    struct addr_pair ap;
};

struct event
{
    u8 event_type;
    union
    {
        struct latency_event le;
        struct drop_event de;
    };
};

#endif
