//
// Created by 廖肇燕 on 2023/1/28.
//

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

void loop(int i) {
    printf("hello, loop %d\n", i);
    fflush(stdout);
}

int main(void) {
    int i = 0;
    while (i < 100000) {
        loop(i ++);
        sleep(1);
    }
    return 0;
}

