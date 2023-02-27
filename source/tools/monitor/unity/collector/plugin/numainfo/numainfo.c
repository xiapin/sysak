//
// Created by muya.
//

#include "numainfo.h"
#include <stdio.h>
#include <numa.h>

int init(void * arg) {
    printf("numainfo plugin install, proc: %s\n", get_unity_proc());
    return 0;
}

int call(int t, struct unity_lines* lines) {

    // get numa node number
    // yum install numactl-devel
    int num_nodes = numa_max_node() + 1;
    // num_nodes = 2;
    // read from /sys/devices/system/node/node0/numastat
    // printf("numa %d\n", num_nodes);
    struct unity_line* line;
    int i, j, ret;
	FILE *fp;
	char fname[128];

    unity_alloc_lines(lines, num_nodes);    // 预分配好
    
    // unity_set_index(line, 0, "mode", "numa_num");
    // unity_set_value(line, 0, "numa_num_sum", num_nodes);

    for (i = 0; i < num_nodes; i++) {
        char numa_name[10];
        snprintf(numa_name, 10, "%s%d", "node", i);
        // printf("numa is %s\n", numa_name);
        line = unity_get_line(lines, i);
        unity_set_table(line, "numainfo");
        unity_set_index(line, 0, "node", numa_name);
        fp = NULL;
        errno = 0;
        if (sprintf(fname, "%s%s%d%s", get_unity_proc(), "/sys/devices/system/node/node", i, "/numastat") < 0)
            printf("sprintf error\n");
        // printf("fname is %s\n", fname);
        if ((fp = fopen(fname, "r")) == NULL) {
            ret = errno;
            printf("WARN: numainfo install FAIL fopen\n");
            return ret;
        }
        for (j = 0; j < 6; j++) {
            char k[32];
            unsigned long v;
            errno = fscanf(fp, "%s %ld\n", k, &v);
            if (errno < 0)
                return errno;
            // printf("k is %s\n", k);
            unity_set_value(line, j, k, v);
        }
        if (fp)
		    fclose(fp);
    }
    return 0;
}

void deinit(void) {
    printf("sample plugin uninstall\n");
}
