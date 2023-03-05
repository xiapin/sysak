//
// Created by 廖肇燕 on 2023/2/15.
//

#include "unity_interface.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


#define UNITY_PATH_LEN 64
static char unity_proc[UNITY_PATH_LEN];
static char unity_sys[UNITY_PATH_LEN];

static void check_path(const char *path) {
    int len = strlen(path);
    if (len > UNITY_PATH_LEN - 1) {
        fprintf(stderr, "set path len %d, overlimit than %d\n", len, UNITY_PATH_LEN);
        exit(1);
    }
}

void set_unity_proc(const char *path) {
    check_path(path);
    strncpy(unity_proc, path, UNITY_PATH_LEN);
}

void set_unity_sys(const char *path) {
    check_path(path);
    strncpy(unity_sys, path, UNITY_PATH_LEN);
}

char *get_unity_proc(void) {
    return unity_proc;
}

char *get_unity_sys(void) {
    return unity_sys;
}