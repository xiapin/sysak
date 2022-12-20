//
// Created by 廖肇燕 on 2022/12/20.
//

#include <stdio.h>
#include <stdlib.h>
#include "beaver.h"

int main(int argc, char** argv) {
    int port = 8400;
    int thread = 3;
    if (argc >= 3) {
        char *ptr;
        port = strtol(argv[1], &ptr, 10);
        thread = strtol(argv[2], &ptr, 10);
    }
    beaver_init(port, thread);
}

