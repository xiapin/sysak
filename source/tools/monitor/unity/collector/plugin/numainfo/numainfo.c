//
// Created by muya.
//

#include "numainfo.h"
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>

int init(void *arg)
{
    printf("numainfo plugin install, proc: %s\n", get_unity_proc());
    return 0;
}

#define NODE_DIR "/sys/devices/system/node/"

int get_numa_node_count(char *path)
{
    int count = 0;
    DIR *dir;
    struct dirent *entry;
    char fname[128];
    sprintf(fname, "%s%s", path, NODE_DIR);
    if ((dir = opendir(fname)) == NULL)
    {
        perror("打开/sys/devices/system/node/目录失败");
        exit(EXIT_FAILURE);
    }
    while ((entry = readdir(dir)) != NULL)
    {
        if (strncmp(entry->d_name, "node", 4) == 0)
        {
            count++;
        }
    }
    closedir(dir);
    return count;
}
// how to test
//  sh-4.2# mkdir -p /tmp/sys/devices/system/node/node0/
//  sh-4.2# cp -r /sys/devices/system/node/node0/numastat /tmp/sys/devices/system/node/node0/
//  sh-4.2# cp -r /sys/devices/system/node/node0/meminfo /tmp/sys/devices/system/node/node0/
//  sh-4.2# mkdir -p /tmp/sys/devices/system/node/node1/
//  sh-4.2# cp -r /sys/devices/system/node/node0/numastat /tmp/sys/devices/system/node/node1/
//  sh-4.2# cp -r /sys/devices/system/node/node0/meminfo /tmp/sys/devices/system/node/node1/
// 

int call(int t, struct unity_lines *lines)
{

    // get numa node number
    int num_nodes = get_numa_node_count(get_unity_proc());
    // num_nodes = 2;
    // read from /sys/devices/system/node/node0/numastat
    // printf("numa %d\n", num_nodes);
    struct unity_line *line;
    int i, j, ret;
    FILE *fp;
    char fname[128];

    unity_alloc_lines(lines, num_nodes); // 预分配好

    // unity_set_index(line, 0, "mode", "numa_num");
    // unity_set_value(line, 0, "numa_num_sum", num_nodes);

    for (i = 0; i < num_nodes; i++)
    {
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
        if ((fp = fopen(fname, "r")) == NULL)
        {
            ret = errno;
            printf("WARN: numainfo install FAIL fopen\n");
            return ret;
        }
        for (j = 0; j < 6; j++)
        {
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

        if (sprintf(fname, "%s%s%d%s", get_unity_proc(), "/sys/devices/system/node/node", i, "/meminfo") < 0)
            printf("sprintf error\n");
        if ((fp = fopen(fname, "r")) == NULL)
        {
            ret = errno;
            printf("WARN: numainfo install FAIL fopen\n");
            return ret;
        }
        char buf[1024];
        int mem_total = 0, mem_free = 0;
        int node_id = 0;
        while (fgets(buf, sizeof(buf), fp) != NULL)
        {
            if (sscanf(buf, "Node %d MemTotal: %d kB", &node_id, &mem_total) == 2)
            {
                // 读取到了 MemTotal 的值
                // printf("总内存：%d kB\n", mem_total);
                unity_set_value(line, j, "node_mem_total", mem_total);
            }
            else if (sscanf(buf, "Node %d MemFree: %d kB", &node_id, &mem_free) == 2)
            {
                // 读取到了 MemFree 的值
                // printf("空闲内存：%d kB\n", mem_free);
                unity_set_value(line, j + 1, "node_mem_free", mem_free);
            }
        }
        if (fp)
            fclose(fp);
    }
    return 0;
}

void deinit(void)
{
    printf("numainfo plugin uninstall\n");
}
