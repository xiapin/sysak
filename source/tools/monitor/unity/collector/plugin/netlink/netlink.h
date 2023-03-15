#ifndef __NET_LINK_H
#define __NET_LINK_H

#include "../plugin_head.h"

int init(void * arg);
int call(int t, struct unity_lines* lines);
void deinit(void);


#endif 