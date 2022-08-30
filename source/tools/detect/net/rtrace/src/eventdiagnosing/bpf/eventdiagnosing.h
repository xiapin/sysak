#ifndef __EVENT_DIAGNOSING_H
#define __EVENT_DIAGNOSING_H

#include "common.h"

enum
{
    TRIGGER_SCHED_IN,
    TRIGGER_SCHED_OUT,
    TRIGGER_VRING_INTERRUPT,
    TRIGGER_KVMEXIT,
};

struct trigger
{
    u8 type;
    // u16 cpu; // percpu array
    u32 pid;
    u64 ts;
};

#endif
