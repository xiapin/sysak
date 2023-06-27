#ifndef UNITY_PMU_EVENT_H
#define UNITY_PMU_EVENT_H
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <error.h>
#include <errno.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include "../plugin_head.h"
#define NR_EVENTS	3

int init(void * arg);
int call(int t, struct unity_lines* lines);
void deinit(void);

#endif //UNITY_PMU_EVENT_H
