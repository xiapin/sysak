
#ifndef __RTRACE_ABNORMAL_H
#define __RTRACE_ABNORMAL_H


#define SEQ_START_TOKEN ((void *)1)

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


struct tcp_params
{
    u8 state;

    // queue
    // length of accept queue
    u32 sk_ack_backlog;
    // length of syn queue
    u32 icsk_accept_queue;
    u32 sk_max_ack_backlog;

    // memory
    u32 sk_wmem_queued;
    u32 sndbuf;
    u32 rmem_alloc;
    u32 sk_rcvbuf;

    u32 drop;
    u32 retran;
    u32 ooo;

};

struct net_params
{
    // netns conntrack stat   
    u32 insert_failed;
};

struct addr_pair
{
    u32 saddr;
    u32 daddr;
    u16 sport;
    u16 dport;
};

struct filter
{
    u32 pid;
    u16 protocol;
    u64 ts;
    struct addr_pair ap;
};

struct event
{
    u16 protocol;
    u8 has_net;
    u32 i_ino;

    struct addr_pair ap;

    struct net_params np;

    union
    {
        struct tcp_params tp;
    };
};

#endif
