//
// Created by muya.
//

#include "cpufreq.h"
#include <stdio.h>

int init(void * arg) {
    printf("cpufreq plugin install, proc: %s\n", get_unity_proc());
    return 0;
}

int call(int t, struct unity_lines* lines) {
    struct unity_line* line;
    int ret;
	FILE *fp = NULL;
    char str[128];
    int len = 0;
    char result[16] = {0};

    unity_alloc_lines(lines, 1);
    line = unity_get_line(lines, 0);
    unity_set_table(line, "cpufreq");

    errno = 0;
    
    if ((fp = fopen("/proc/cpuinfo", "r")) == NULL) {
        ret = errno;
        printf("WARN: numainfo install FAIL fopen\n");
        return ret;
    }
    while (fgets(str, sizeof(str), fp)) {
        char *pLast = strstr(str, "@");
        if (NULL != pLast) {
            pLast = pLast + 2;
            while (*pLast != 'G')
            {
                len++;
                pLast++;
            }
            memcpy(result, pLast-len, len);
            // printf("res is %s\n", result);
            unity_set_value(line, 0, "hardware_freq", atof(result)*1000);
            memset(result, 0, 16);
            len = 0;
        } else {
            char *pLast = strstr(str, "MHz");
            char *pLast2 = strstr(str, ":");
            if (NULL != pLast && NULL != pLast2) {
                pLast2 = pLast2 + 2;
                while (*pLast2 != '\n')
                {
                    len++;
                    pLast2++;
                }
                memcpy(result, pLast2-len, len);
                // printf("res2 is %s, %d\n", result, len);
                // printf("res22 is %s\n", str);
                unity_set_value(line, 1, "curr_freq", atof(result));
                memset(result, 0, 16);
                len = 0;
                break;
            }
        }
    }
    if (fp)
        fclose(fp);
    return 0;
}

void deinit(void) {
    printf("cpufreq plugin uninstall\n");
}
