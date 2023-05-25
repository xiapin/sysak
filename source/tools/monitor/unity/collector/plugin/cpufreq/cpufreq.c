//
// Created by muya.
//
#include "cpufreq.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

long nr_cpus;
int init(void * arg) {
	int ret;

	nr_cpus = sysconf(_SC_NPROCESSORS_CONF);
	if (nr_cpus < 0) {
		ret = errno;
		printf("WARN: cpufreq plugin install FAIL sysconf\n");
		return ret;
	}
    printf("cpufreq plugin install, proc: %s\n", get_unity_proc());
    return 0;
}

int call(int t, struct unity_lines* lines) {
    struct unity_line* line;
    int ret;
	int num_line, cpu_hw, cpu_cur;
	FILE *fp = NULL;
	char cpu_name[16] = {0};
    char str[128];
    int len = 0;
    char result[16] = {0};

    unity_alloc_lines(lines, nr_cpus*2);

    errno = 0;
    
    if ((fp = fopen("/proc/cpuinfo", "r")) == NULL) {
        ret = errno;
        printf("WARN: numainfo install FAIL fopen\n");
        return ret;
    }

	num_line = cpu_hw = cpu_cur = 0;
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
		snprintf(cpu_name, sizeof(cpu_name), "%d", cpu_hw++);
    		line = unity_get_line(lines, num_line++);
   		unity_set_table(line, "cpufreq");
		unity_set_index(line, 0, "core", cpu_name);
		unity_set_value(line, 0, "hwFreq", atof(result)*1000);
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
		snprintf(cpu_name, sizeof(cpu_name), "%d", cpu_cur++);
    		line = unity_get_line(lines, num_line++);
   		unity_set_table(line, "cpufreq");
		unity_set_index(line, 0, "core", cpu_name);
                unity_set_value(line, 0, "curFreq", atof(result));
                memset(result, 0, 16);
                len = 0;
                //break;
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
