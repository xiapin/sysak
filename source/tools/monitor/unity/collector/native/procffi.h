//
// Created by 廖肇燕 on 2022/12/16.
//

#ifndef UNITY_PROCFFI_H
#define UNITY_PROCFFI_H

#define VAR_INDEX_MAX 64

typedef struct var_long {
    int no;
    long long value[VAR_INDEX_MAX];
}var_long_t;

typedef struct var_string {
    int no;
    char s[VAR_INDEX_MAX][32];
}var_string_t;

typedef struct var_kvs {
    int no;
    char s[32];
    long long value[VAR_INDEX_MAX];
}var_kvs_t;

int var_input_long(const char * line, struct var_long *p);
int var_input_string(const char * line, struct var_string *p);
int var_input_kvs(const char * line, struct var_kvs *p);

#endif //UNITY_PROCFFI_H
