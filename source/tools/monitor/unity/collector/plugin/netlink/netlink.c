#include "netlink.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static char buffer[4096];

int get_conntrack_drop()
{
    int total_drop = 0, i;
    FILE *fp = NULL;
    fp = popen("conntrack -S", "r"); // 将命令ls-l 同过管道读到fp

    if (!fp)
        return -1;

    while (fgets(buffer, 4096, fp) != NULL)
    {
        char *buf = buffer;
        while ((buf = strstr(buf, " drop=")) != NULL)
        {
            buf += strlen(" drop=");
            for (i = 0;; i++)
            {
                if (buf[i] > '9' || buf[i] < '0')
                {
                    buf[i] = 0;
                    break;
                }
            }
            total_drop += atoi(buf);
            buf += i + 1;
        }
    }
    pclose(fp);
    return total_drop;
}

int get_tc_drop()
{
    int total_drop = 0, i;
    FILE *fp = NULL;
    fp = popen("tc -s qdisc", "r"); // 将命令ls-l 同过管道读到fp

    if (!fp)
        return -1;

    while (fgets(buffer, 4096, fp) != NULL)
    {
        char *buf = buffer;
        while ((buf = strstr(buf, "dropped ")) != NULL)
        {
            buf += strlen("dropped ");
            for (i = 0;; i++)
            {
                if (buf[i] > '9' || buf[i] < '0')
                {
                    buf[i] = 0;
                    break;
                }
            }
            total_drop += atoi(buf);
            buf += i + 1;
        }
    }
    pclose(fp);
    return total_drop;
}


int init(void * arg) {
    printf("netlink plugin install\n");
    return 0;
}

int call(int t, struct unity_lines* lines) {
    struct unity_line* line;

    unity_alloc_lines(lines, 3);    // 预分配好
    line = unity_get_line(lines, 0);
    unity_set_table(line, "netlink");
    unity_set_value(line, 0, "conntrack_drop", get_conntrack_drop());
    unity_set_value(line, 1, "tc_drop", get_tc_drop());

    return 0;
}

void deinit(void) {
    printf("netlink plugin uninstall\n");
}
