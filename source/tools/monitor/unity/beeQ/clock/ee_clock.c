//
// 高效时钟获取方案
// Created by 廖肇燕 on 2023/3/17.
//

#include "ee_clock.h"

typedef unsigned long long cycles_t;

inline cycles_t currentcycles() {
    cycles_t result;
    __asm__ __volatile__ ("rdtsc" : "=A" (result));
    return result;
}
