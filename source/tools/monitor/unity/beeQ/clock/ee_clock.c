//
// 高效时钟获取方案
// Created by 廖肇燕 on 2023/3/17.
//

#include "ee_clock.h"
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

#define TIME_SECOND_UNIT 100000UL  // 睡眠校准时间，
#define MICRO_UNIT (1000 * 1000UL)

static ee_clock_t clk_coef = 0;

static ee_clock_t get_cycles() {
    unsigned int hi, lo;
    ee_clock_t res;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    res = ((ee_clock_t)lo) | (((ee_clock_t)hi) << 32);
    return res;
}

ee_clock_t get_native_us(void) {
    ee_clock_t res = 0;
    struct timeval tv;

    if (gettimeofday(&tv, NULL) == 0) {
        res = tv.tv_sec * MICRO_UNIT + tv.tv_usec;
    }
    return res;
}

// 校准时钟
int calibrate_local_clock(){
    ee_clock_t coef1, coef2;
    ee_clock_t t1, t2;
    ee_clock_t ts1, ts2;
    ee_clock_t delta1, delta2;
    ee_clock_t dts1, dts2;
    ee_clock_t res;

    t1 = get_cycles();
    ts1 = get_native_us();
    usleep(TIME_SECOND_UNIT);
    t2 = get_cycles();
    ts2 = get_native_us();
    delta1 = t2 - t1;
    dts1 = ts2 - ts1;

    t1 = get_cycles();
    ts1 = get_native_us();
    usleep(TIME_SECOND_UNIT);
    t2 = get_cycles();
    ts2 = get_native_us();
    delta2 = t2 - t1;
    dts2 = ts2 - ts1;

    coef1 = delta1 / dts2;
    coef2 = delta2 / dts1;

    if (coef1 <= 100 || coef2 <= 100) {
        fprintf(stderr, "read clock too small.\n");
        return -EIO;
    }

    res = 100 * coef1 / coef2;
    if (res >= 110 || res <= 90) {
        fprintf(stderr, "calibrate local clock failed.\n");
        fprintf(stderr, "delta1: %ld, delta2: %ld, dts1: %ld, dts2: %ld.\n", delta1, delta2, dts1, dts2);
        return -EIO;
    }

    clk_coef = (coef1 + coef2) / 2;
    return 0;
}

ee_clock_t get_local_clock() {
    return get_cycles() / clk_coef;
}
