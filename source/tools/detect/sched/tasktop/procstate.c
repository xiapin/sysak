#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "procstate.h"
#include "tasktop.h"

double d_placeholder;
int i_placeholder;

int runnable_proc(struct sys_record_t *sys) {
    struct loadavg_t avg;
    int err = 0;
    FILE *fp = fopen(LOADAVG_PATH, "r");
    if (!fp) {
        fprintf(stderr, "Failed open %s.", LOADAVG_PATH);
        err = errno;
        return err;
    }

    fscanf(fp, "%f %f %f %d/%d %ld", &avg.load1, &avg.load5, &avg.load15, &avg.nr_running,
           &avg.nr_threads, &avg.new_pid);

    if (avg.nr_running > 0) avg.nr_running--;

    sys->load1 = avg.load1;
    sys->nr_R = avg.nr_running;
    // printf("load1 = %.2f load5 = %.2f load15 = %.2f\n", avg.load1, avg.load5, avg.load15);
    return err;
}

int unint_proc(struct sys_record_t *sys) {
    int err = 0;
    FILE *fp;
    long long total_unint = 0, nr_unint;

    char line[1024];

    if ((fp = fopen(SCHED_DEBUG_PATH, "r")) == NULL) {
        err = -errno;
        fprintf(stderr, "Failed open %s\n", SCHED_DEBUG_PATH);
        return err;
    }

    while (fgets(line, 1024, fp) != NULL) {
        if (!strncmp(line, "  .nr_uninterruptible", 21)) {
            sscanf(line + 35, "%lld", &nr_unint);
            // printf("read nr_unint=%lld\n", nr_unint);
            total_unint += nr_unint;
        }
    }
    if (total_unint < 0) total_unint = 0;

    sys->nr_D = total_unint;
    if (fclose(fp)) fprintf(stderr, "Failed fclose %s.\n", SCHED_DEBUG_PATH);
    return err;
}