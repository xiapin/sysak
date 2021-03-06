#ifndef __MEMLEAK_IOCTL__
#define __MEMLEAK_IOCTL__
#include <linux/ioctl.h>

#include "common.h"

#define MEMLEAK_CMD_ENALBE (0x0A)
#define MEMLEAK_CMD_SET (MEMLEAK_CMD_ENALBE + 1)
#define MEMLEAK_CMD_GET (MEMLEAK_CMD_SET + 1)
#define MEMLEAK_CMD_RESULT (MEMLEAK_CMD_GET + 1)
#define MEMLEAK_CMD_DISABLE (MEMLEAK_CMD_RESULT + 1)

#define MEMLEAK_STATE_ON (1)
#define MEMLEAK_STATE_OFF (2)
#define MEMLEAK_STATE_INIT (3)

#define MEMLEAK_ON  _IOWR(MEMLEAK_IOCTL_CMD, MEMLEAK_CMD_ENALBE, struct memleak_settings)
#define MEMLEAK_OFF  _IO(MEMLEAK_IOCTL_CMD, MEMLEAK_CMD_DISABLE)
#define MEMLEAK_RESULT  _IOWR(MEMLEAK_IOCTL_CMD, MEMLEAK_CMD_RESULT, struct user_result)

#endif
