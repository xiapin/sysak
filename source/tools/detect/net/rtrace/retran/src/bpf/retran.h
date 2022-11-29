#ifndef __RETRAN_H
#define __RETRAN_H
#include "common.h"




enum {
    RTO_RETRAN,
    FAST_RETRAN,
};


struct retran_event {
    u8 tcp_state;
    u8 ca_state;
    u8 retran_times;
    u64 ts;

    struct addr_pair ap;
    
};

#endif
