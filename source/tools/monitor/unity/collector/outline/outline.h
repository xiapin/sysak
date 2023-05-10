//
// Created by 廖肇燕 on 2023/2/16.
//

#ifndef UNITY_OUTLINE_H
#define UNITY_OUTLINE_H

#include "../../beeQ/beeQ.h"
#include <lauxlib.h>
#include <lualib.h>
pthread_t outline_init(struct beeQ* pushQ, char *fYaml);

#endif //UNITY_OUTLINE_H
