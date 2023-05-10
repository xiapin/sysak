#ifndef __ICMP_H
#define __ICMP_H
#include "common.h"

enum
{
    PING_SND = 0,
    PING_NET_DEV_QUEUE,
    PING_NET_DEV_XMIT,
    PING_DEV_RCV,
    PING_NETIF_RCV,
    PING_ICMP_RCV,
    PING_RCV,
};

struct icmp_event
{
    u8 type;
    // echo or reply
    u8 icmp_type;
    u16 cpu;
    u16 seq;
    u16 id;
    u64 ts;
    u64 skb_ts;
    u32 pid;
    u8 comm[16];
};

#endif
