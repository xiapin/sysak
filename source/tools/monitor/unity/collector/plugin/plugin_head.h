//
// Created by 廖肇燕 on 2022/12/30.
//

#ifndef UNITY_PLUGIN_HEAD_H
#define UNITY_PLUGIN_HEAD_H

struct unity_index {
    char name[16];
    char index[16];
};

struct unity_value {
    char name[16];
    double value;
};

struct unity_log {
    char name[16];
    char* log;
};

struct unity_line {
    char table[32];
    struct unity_index indexs[4];
    struct unity_value values[32];
    struct unity_log logs[1];
};

struct unity_lines {
    int num;
    struct unity_line *line;
};

#include <stdlib.h>   // for malloc exit
#include <string.h>   // for memset
#include <stdio.h>
#include <errno.h>
#include "../../beeQ/beeQ.h"
#include "../native/sig_stop.h"
#include "../native/unity_interface.h"

inline struct unity_lines *unity_new_lines(void) __attribute__((always_inline));
inline int unity_alloc_lines(struct unity_lines * lines, unsigned int num) __attribute__((always_inline));
inline struct unity_line * unity_get_line(struct unity_lines * lines, unsigned int i) __attribute__((always_inline));
inline int unity_set_table(struct unity_line * line, const char * table) __attribute__((always_inline));
inline int unity_set_index(struct unity_line * line, unsigned int i, const char * name, const char * index) __attribute__((always_inline));
inline int unity_set_value(struct unity_line * line, unsigned int i, const char * name, double value) __attribute__((always_inline));
inline int unity_set_log(struct unity_line * line, const char * name, const char * log) __attribute__((always_inline));

inline struct unity_lines *unity_new_lines(void) {
    return malloc(sizeof (struct unity_lines));
}

inline int unity_alloc_lines(struct unity_lines * lines, unsigned int num) {
    size_t size = num * sizeof (struct unity_line);
    lines->line = (struct unity_line *)(malloc(size));

    if (lines->line == NULL) {
        perror("alloc memory for unity line failed.");
        exit(1);
    }
    memset(lines->line, 0, size);
    lines->num = num;
    return num;
}

inline struct unity_line * unity_get_line(struct unity_lines * lines, unsigned int i) {
    if (i >= 4) {
        return NULL;
    }
    return &(lines->line[i]);
}

inline int unity_set_table(struct unity_line * line, const char * table) {
    strncpy(line->table, table, 32);
    return 0;
}

inline int unity_set_index(struct unity_line * line,
                           unsigned int i, const char * name, const char * index) {
    if (i >= 4) {
        return -ERANGE;
    }
    strncpy(line->indexs[i].name, name, 16);
    strncpy(line->indexs[i].index, index, 16);
    return 0;
}

inline int unity_set_value(struct unity_line * line,
                           unsigned int i, const char * name, double value) {
    if (i >= 32) {
        return -ERANGE;
    }
    strncpy(line->values[i].name, name, 16);
    line->values[i].value = value;
    return 0;
}

inline int unity_set_log(struct unity_line * line,
                        const char * name, const char * log) {
    strncpy(line->logs[0].name, name, 16);
    line->logs[0].log = strdup(log);
    return 0;
}

#endif //UNITY_PLUGIN_HEAD_H
