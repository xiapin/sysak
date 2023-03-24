//
// Created by 廖肇燕 on 2023/3/24.
//

#ifndef UNITY_POSTQUE_H
#define UNITY_POSTQUE_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

int postQue_pull(char *msg);
int postQue_post(char *msg);
int postQue_init();

#endif //UNITY_POSTQUE_H
