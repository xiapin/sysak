#include <stdio.h>
#include <limits.h>
#include <stdint.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>

#include "rdt_helper.h"

#define MB (1024 * 1024)

/* if out of range or no number found return nonzero */
static int parse_ull(const char* str, uint64_t* retval) {
    // printf("input str=[%s]\n", str);
    int err = 0;
    char* endptr;
    errno = 0;
    unsigned long long int val = strtoull(str, &endptr, 10);
    // printf("parse val=%llu\n", val);

    /* Check for various possible errors */
    if (errno == ERANGE) {
        fprintf(stderr, "Failed parse val.\n");
        err = errno;
        return err;
    }

    if (endptr == str) return err = -1;
    *retval = val;
    return err;
}

int calculate(const char* now, const char* prev) {
    uint64_t ret = 0;
    // printf("hello now=%s, prev=%s\n", now, prev);
    uint64_t now_val = 0, prev_val = 0;
    parse_ull(now, &now_val);
    parse_ull(prev, &prev_val);

    // printf("now_val =%llu prev_val =%llu\n", now_val, prev_val);
    if (prev_val) {
        ret = now_val > prev_val ? now_val - prev_val
                                 : now_val + UINT64_MAX - prev_val;
        ret = ret / MB;
    }
    // printf("res=%llu\n", ret);
    return ret;
}