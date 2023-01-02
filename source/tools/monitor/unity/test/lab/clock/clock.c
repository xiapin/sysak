//
// Created by 廖肇燕 on 2022/12/31.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

typedef unsigned long bee_time_t;

static bee_time_t local_time(void) {
    int ret;
    struct timespec tp;

    ret = clock_gettime(CLOCK_MONOTONIC, &tp);
    if (ret == 0) {
        return tp.tv_sec * 1000000 + tp.tv_nsec / 1000;
    } else {
        perror("get clock failed.");
        exit(1);
        return 0;
    }
}

void loop(int loop) {
    for (int i = 0; i < loop; ++i) {
        for (int j = 0; j < loop; ++j) {
            j=j;
        }
    }
}

void test(int l) {
    bee_time_t t1, t2, t3, delta;
    t1 = local_time();
    loop(l);
    t2 = local_time();
    delta = t1 + 1000000 - t2;
    usleep(delta);
    t3 = local_time();
    printf("delta: %ld, %ld\n", t3 - t1, delta);
}

int main(void) {
    test(1000);
    test(2000);
    test(4000);
    test(8000);
    test(9000);
    return 0;
}

