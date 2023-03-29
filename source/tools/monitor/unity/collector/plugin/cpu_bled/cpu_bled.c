//
// Created by 廖肇燕 on 2023/3/21.
//

#include "cpu_bled.h"
#include <stdio.h>
#include <time.h>

static int bled_wait = 0;

int init(void * arg) {
    printf("setup for cpu_bled.\n");
    return 0;
}

int call(int t, struct unity_lines* lines) {
    bled_wait ++;
    if (bled_wait > 10) {
        printf("inject cpu bled.\n");
        time_t now = time(NULL) + 3;
        while (time(NULL) <= now);
    }
    return 0;
}

void deinit(void) {
    printf("cpu bled uninstall\n");
}
