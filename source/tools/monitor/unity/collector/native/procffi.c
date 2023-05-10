//
// Created by 廖肇燕 on 2022/12/16.
//

#include "procffi.h"

#include <stdio.h>
#include <string.h>

static inline int var_next(char ** ppv) {
    char *pv = *ppv;

    if (pv == NULL) {
        return -1;
    }
    while (*pv == ' ') {
        pv ++;
    }
    if (*pv == '\0') {
        return -1;
    }

    *ppv = pv;
    return 0;
}

int var_input_long(const char * line, struct var_long *p) {
    char *pv = (char *)line;
    int res;
    int i;

    p->no = 0;
    if (var_next(&pv)) {
        return 0;
    }

    for (i = 0; i < VAR_INDEX_MAX; i ++) {
        res = sscanf(pv, "%lld", &p->value[i]);
        if (!res) {
            goto breakLoop;
        }

        pv = strchr(pv, ' ');
        if (var_next(&pv)) {
            goto breakLoop;
        }
    }
    p->no = i;
    return 0;
    breakLoop:
    p->no = i + 1;
    return 0;
}

int var_input_string(const char * line, struct var_string *p) {
    char *pv = (char *)line;
    int res;
    int i;

    p->no = 0;
    if (var_next(&pv)) {
        return 0;
    }

    for (i = 0; i < VAR_INDEX_MAX; i ++)  {
        res = sscanf(pv, "%31s", &p->s[i][0]);
        if (!res) {
            goto breakLoop;;
        }

        pv = strchr(pv, ' ');
        if (var_next(&pv)) {
            goto breakLoop;
        }
    }
    p->no = i;
    return 0;
    breakLoop:
    p->no = i + 1;
    return 0;
}

int var_input_kvs(const char * line, struct var_kvs *p) {
    char *pv = (char *)line;
    int res;
    int i;

    p->no = 0;
    if (var_next(&pv)) {
        return 0;
    }

    res = sscanf(pv, "%31s", &p->s[0]);
    if (!res) {
        return -1;
    }

    pv = strchr(pv, ' ');
    if (var_next(&pv)) {
        return 0;
    }

    for (i = 0; i < VAR_INDEX_MAX; i ++)  {
        res = sscanf(pv, "%lld", &p->value[i]);
        if (!res) {
            goto breakLoop;
        }

        pv = strchr(pv, ' ');
        if (var_next(&pv)) {
            goto breakLoop;
        }
    }
    p->no = i;
    return 0;
    breakLoop:
    p->no = i + 1;
    return 0;
}
