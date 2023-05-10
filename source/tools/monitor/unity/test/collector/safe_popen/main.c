//
// Created by 廖肇燕 on 2023/1/28.
//

#include <stdio.h>
#include "safe_popen.h"

int main(void) {
    char s[1024];

    safe_popen_read("./loop", s, 1024, 10000);
    printf("s: %s", s);
    return 0;
}

