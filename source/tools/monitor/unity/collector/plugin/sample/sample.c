//
// Created by 廖肇燕 on 2022/12/30.
//

#include "sample.h"

int init(void * arg) {
    printf("sample plugin install, proc: %s\n", get_unity_proc());
    return 0;
}

int call(int t, struct unity_lines* lines) {
    static double value = 0.0;
    struct unity_line* line;

    unity_alloc_lines(lines, 3);    // 预分配好
    line = unity_get_line(lines, 0);
    unity_set_table(line, "sample_tbl1");
    unity_set_index(line, 0, "mode", "sample1");
    unity_set_value(line, 0, "value1", 1.0 + value);
    unity_set_value(line, 1, "value2", 2.0 + value);

    line = unity_get_line(lines, 1);
    unity_set_table(line, "sample_tbl1");
    unity_set_index(line, 0, "mode", "sample2");
    unity_set_value(line, 0, "value1", 1.2 + value);
    unity_set_value(line, 1, "value2", 2.2 + value);

    line = unity_get_line(lines, 2);
    unity_set_table(line, "sample_tbl2");
    unity_set_value(line, 0, "value1", 3.0 + value);
    unity_set_value(line, 1, "value2", 4.0 + value);
    unity_set_value(line, 2, "value3", 3.1 + value);
    unity_set_value(line, 3, "value4", 4.1 + value);

    value += 0.1;
    return 0;
}

void deinit(void) {
    printf("sample plugin uninstall\n");
}
