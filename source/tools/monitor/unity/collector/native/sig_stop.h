//
// Created by 廖肇燕 on 2023/1/28.
//

#ifndef UNITY_SIG_STOP_H
#define UNITY_SIG_STOP_H

#include <pthread.h>

int plugin_is_working(void);
void plugin_thread_stop(pthread_t tid);
void plugin_stop(void);
void plugin_init(void);

#endif //UNITY_SIG_STOP_H
