//
// Created by muya.
//

#include "gpuinfo.h"
#include <stdio.h>


int init(void * arg) {
    printf("gpuinfo plugin install, proc: %s\n", get_unity_proc());
    return 0;
}

int call(int t, struct unity_lines* lines) {



    FILE *fp = NULL;
    char buffer[256];       /* Temporary buffer for parsing */
    float mm_total, mm_used, mm_free, temp, powerdraw, gpu_util, mem_util;
    struct unity_line* line;


    // make sure nvidia-smi installed
    // if use container, use -v /usr/bin/nvidia-smi:/usr/bin/nvidia-smi
    if ( access("/usr/bin/nvidia-smi",0) ) {
        // printf("nvidia-smi not exists\n");
        return 0;
    }

	fp = popen("nvidia-smi --query-gpu=\"memory.total,memory.used,memory.free,temperature.gpu,power.draw,utilization.gpu,utilization.memory\" --format=nounits,csv,noheader", "r");
	memset(buffer, 0, sizeof(buffer));

    // // for test
    // char command[128];
    // if (sprintf(command, "cat %s%s", get_unity_proc(), "/proc/gpuinfo") < 0)
    //     printf("sprintf error\n");
    // fp = popen(command, "r");


    if (fp != NULL)
    {
        while (fgets(buffer, sizeof(buffer), fp))
        {
            sscanf(buffer, "%f, %f, %f, %f, %f, %f, %f",  &mm_total, &mm_used, &mm_free, &temp, &powerdraw, &gpu_util, &mem_util);
        }
        pclose(fp);
    }

    unity_alloc_lines(lines, 1);    // 预分配好
    line = unity_get_line(lines, 0);
    unity_set_table(line, "gpuinfo");
    unity_set_index(line, 0, "gpu_num", "gpu0");
    unity_set_value(line, 0, "mm_total", mm_total);
    unity_set_value(line, 1, "mm_used", mm_used);
    unity_set_value(line, 2, "mm_free", mm_free);
    unity_set_value(line, 3, "temp", temp);
    unity_set_value(line, 4, "powerdraw", powerdraw);
    unity_set_value(line, 5, "gpu_util", gpu_util);
    unity_set_value(line, 6, "mem_util", mem_util);

    return 0;
}

void deinit(void) {
    printf("gpuinfo plugin uninstall\n");
}
