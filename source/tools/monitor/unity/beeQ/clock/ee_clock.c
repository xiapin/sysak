//
// 高效时钟获取方案
// Created by 廖肇燕 on 2023/3/17.
//

#include "ee_clock.h"
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#define TIME_SECOND_UNIT 100000UL

static ee_clock_t clk_coef = 0;

static ee_clock_t get_cycles() {
    unsigned int hi, lo;
    ee_clock_t res;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    res = ((ee_clock_t)lo) | (((ee_clock_t)hi) << 32);
    return res;
}

// 校准时钟
int calibrate_local_clock(){
    ee_clock_t coef1, coef2;
    ee_clock_t t1, t2, t3;
    ee_clock_t delta1, delta2;
    ee_clock_t res;

    t1 = get_cycles();
    usleep(TIME_SECOND_UNIT);
    t2 = get_cycles();
    usleep(TIME_SECOND_UNIT);
    t3 = get_cycles();

    delta1 = t2 - t1;
    delta2 = t3 - t2;

    coef1 = delta1 / TIME_SECOND_UNIT;
    coef2 = delta2 / TIME_SECOND_UNIT;

    if (coef1 <= 100 || coef2 <= 100) {
        fprintf(stderr, "read clock too small.\n");
        return -EIO;
    }

    res = 100 * coef1 / coef2;
    if (res >= 110 || res <= 90) {
        fprintf(stderr, "calibrate local clock failed.\n");
        return -EIO;
    }

    clk_coef = (coef1 + coef2) / 2;
    return 0;
}

ee_clock_t get_local_clock() {
    return get_cycles() / clk_coef;
}
