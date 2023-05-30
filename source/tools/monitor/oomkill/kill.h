/* SPDX-License-Identifier: MIT */
#ifndef KILL_H
#define KILL_H

#include <regex.h>
#include <stdbool.h>
#include <poll.h>
#include <sys/eventfd.h>
#include "meminfo.h"
#include "metric.h"

#define KILL_MODE_0 (0)
#define KILL_MODE_1 (1)
#define KILL_MODE_2 (2)
#define KILL_MODE_3 (3)

typedef struct {
    /* if the available memory AND swap goes below these percentages,
     * we start killing processes */
    double mem_term_percent;
    double mem_kill_percent;
    double swap_term_percent;
    double swap_kill_percent;
    /* send d-bus notifications? */
    bool notify;
    /* Path to script for programmatic notifications (or NULL) */
    char* notify_ext;
    /* kill all processes within a process group */
    bool kill_process_group;
    /* do not kill processes owned by root */
    bool ignore_root_user;
    /* prefer/avoid killing these processes. NULL = no-op. */
    regex_t* prefer_regex;
    regex_t* avoid_regex;
    /* will ignore these processes. NULL = no-op. */
    regex_t* ignore_regex;
    /* memory report interval, in milliseconds */
    int report_interval_ms;
    /* Flag --dryrun was passed */
    bool dryrun;
    struct cpu_stat cstat_prev;
    struct cpu_util cstat_util;
    meminfo_t m;
    memstatus mode;
    int kill_mode;
    long min;
    long low;
    long high;
    int iowait_thres;
    int sys_thres;
    int poll_fd;
    int eventc_fd;
    int pressure_fd;
    struct pollfd pfd;
} poll_loop_args_t;

struct kill_args {
    long iowait_avg;
    long iowait_thres;
    long sys_avg;
    long sys_thres;
    long kill_mode;
};

void kill_process(const poll_loop_args_t* args, int sig, const procinfo_t* victim);
procinfo_t find_largest_process(const poll_loop_args_t* args);

#endif
