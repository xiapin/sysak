#ifndef __RETRAN_H
#define __RETRAN_H
#include "common.h"




enum {
    SYN_RETRAN,
    SLOW_START_RETRAN,
    RTO_RETRAN,
    FAST_RETRAN,
    TLP,
};


struct retran_event {
    u8 tcp_state;
    u8 ca_state;
    u8 retran_type;
    u64 ts;

    struct addr_pair ap;
    
};

#endif
