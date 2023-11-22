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
    int num_gpus = 0, num_gpus_index = 0;


    // make sure nvidia-smi installed
    // if use container, use -v /usr/bin/nvidia-smi:/usr/bin/nvidia-smi
    if ( access("/usr/bin/nvidia-smi",0) ) {
        // printf("nvidia-smi not exists\n");
        return 0;
    }

	fp = popen("nvidia-smi --query-gpu=\"memory.total,memory.used,memory.free,temperature.gpu,power.draw,utilization.gpu,utilization.memory\" --format=nounits,csv,noheader", "r");
	memset(buffer, 0, sizeof(buffer));

    if (fp != NULL)
    {
        while (fgets(buffer, sizeof(buffer), fp))
        {
            if (strstr(buffer, "Failed") != NULL) {
                // printf("Found the word 'Failed' in the buffer: %s", buffer);
                break;
            }
            num_gpus++;
            sscanf(buffer, "%f, %f, %f, %f, %f, %f, %f",  &mm_total, &mm_used, &mm_free, &temp, &powerdraw, &gpu_util, &mem_util);
        }
        pclose(fp);
    }

    unity_alloc_lines(lines, num_gpus);

    fp = popen("nvidia-smi --query-gpu=\"memory.total,memory.used,memory.free,temperature.gpu,power.draw,utilization.gpu,utilization.memory\" --format=nounits,csv,noheader", "r");
	memset(buffer, 0, sizeof(buffer));

    if (fp != NULL)
    {
        while (fgets(buffer, sizeof(buffer), fp))
        {
            if (strstr(buffer, "Failed") != NULL) {
                // printf("Found the word 'Failed' in the buffer: %s", buffer);
                break;
            }
            
            sscanf(buffer, "%f, %f, %f, %f, %f, %f, %f",  &mm_total, &mm_used, &mm_free, &temp, &powerdraw, &gpu_util, &mem_util);
            line = unity_get_line(lines, num_gpus_index);
            unity_set_table(line, "gpuinfo");
            char gpu_name[10];
            snprintf(gpu_name, 10, "%s%d", "gpu", num_gpus_index);
            unity_set_index(line, 0, "gpu_num", gpu_name);
            unity_set_value(line, 0, "mm_total", mm_total);
            unity_set_value(line, 1, "mm_used", mm_used);
            unity_set_value(line, 2, "mm_free", mm_free);
            unity_set_value(line, 3, "temp", temp);
            unity_set_value(line, 4, "powerdraw", powerdraw);
            unity_set_value(line, 5, "gpu_util", gpu_util);
            unity_set_value(line, 6, "mem_util", mem_util);

            num_gpus_index++;
        }
        pclose(fp);
    }

    return 0;
}

void deinit(void) {
    printf("gpuinfo plugin uninstall\n");
}
