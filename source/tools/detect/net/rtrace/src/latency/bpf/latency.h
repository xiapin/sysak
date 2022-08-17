#ifndef __LATENCY_H
#define __LATENCY_H

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


struct loghist
{
    u32 hist[32];
};

#endif
