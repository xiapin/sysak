#ifndef __RETRAN_H
#define __RETRAN_H

enum
{
    SYN_RETRAN,
    SLOW_START_RETRAN,
    RTO_RETRAN,
    FAST_RETRAN,
    TLP,
};

struct retran_event
{
    unsigned char tcp_state;
    unsigned char ca_state;
    unsigned char retran_type;

    unsigned int saddr;
    unsigned int daddr;
    unsigned short sport;
    unsigned short dport;
};

#endif
