#ifndef __DROP_H
#define __DROP_H

struct drop_filter
{
    unsigned short protocol;
    unsigned int saddr;
    unsigned int daddr;
    unsigned short sport;
    unsigned short dport;
};


struct drop_event
{
    unsigned long long location;
    unsigned short proto;
    unsigned int saddr;
    unsigned int daddr;
    unsigned short sport;
    unsigned short dport;
};

#endif