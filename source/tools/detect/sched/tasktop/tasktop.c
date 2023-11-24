
#include "tasktop.h"

#include <argp.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <linux/types.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "bpf/tasktop.skel.h"
#include "common.h"
#include "procstate.h"

#define NSEC_PER_SECOND (1000 * 1000 * 1000)
#define NSEC_PER_MILLSECOND (1000 * 1000)

char log_dir[FILE_PATH_LEN] = "/var/log/sysak/tasktop";
char default_log_path[FILE_PATH_LEN] = "/var/log/sysak/tasktop/tasktop.log";
time_t btime = 0;
u64 pidmax = 0;
char* log_path = 0;
int nr_cpu;
static volatile sig_atomic_t exiting;

enum Mode { LOAD, BLOCKED };

struct env {
    time_t delay;
    pid_t tid;
    s64 nr_iter;
    s64 stack_limit;
    s64 cgroup_limit;
    s64 limit;
    int run;
    enum sort_type rec_sort;
    enum Mode mode;
    u64 blocked_ms;
    FILE* dest;
    bool thread_mode;
    bool human;
    bool verbose;
    bool kthread;
} env = {.thread_mode = false,
         .delay = 3,
         .tid = -1,
         .human = false,
         .rec_sort = SORT_CPU,
         .nr_iter = LONG_MAX - 1,
         .limit = INT_MAX,
         .stack_limit = 20,
         .cgroup_limit = 20,
         .mode = LOAD,
         .blocked_ms = 3000,
         .verbose = false,
         .dest = 0,
         .kthread = false,
         .run = -1};

const char* argp_program_version = "tasktop 0.1";
const char argp_program_doc[] =
    "Load analyze & D stack catch.\n"
    "\n"

    "USAGE: \n"
    "load analyze: tasktop [--help] [-t] [-p TID] [-d DELAY] [-i ITERATION] "
    "[-s SORT] "
    "[-f LOGFILE] [-l LIMIT] [-H] [-e D-LIMIT]\n"
    "catch D task stack: tasktop [--mode blocked] [--threshold TIME] [--run "
    "TIME]\n"
    "\n"

    "EXAMPLES:\n"
    "1. Load analyze examples:\n"
    "    tasktop            # run forever, display the cpu utilization.\n"
    "    tasktop -t         # display all thread.\n"
    "    tasktop -p 1100    # only display task with pid 1100.\n"
    "    tasktop -d 5       # modify the sample interval.\n"
    "    tasktop -i 3       # output 3 times then exit.\n"
    "    tasktop -s user    # top tasks sorted by user time.\n"
    "    tasktop -l 20      # limit the records number no more than 20.\n"
    "    tasktop -e 10      # limit the d-stack no more than 10, default is "
    "20.\n"
    "    tasktop -H         # output time string, not timestamp.\n"
    "    tasktop -f a.log   # log to a.log.\n"
    "    tasktop -e 10      # most record 10 d-task stack.\n"
    "\n"
    "2. blocked analyze examples:\n"
    "    tasktop --mode blocked --threshold 1000 --run 120 # tasktop run "
    "(120s) catch the task that blocked in D more than (1000 ms)\n"
    "\n";

static const struct argp_option opts[] = {
    {"human", 'H', 0, 0, "Output human-readable time info."},
    {"thread", 't', 0, 0, "Thread mode, default process"},
    {"pid", 'p', "TID", 0, "Specify thread TID"},
    {"delay", 'd', "DELAY", 0, "Sample peroid, default is 3 seconds"},
    {"iter", 'i', "ITERATION", 0, "Output times, default run forever"},
    {"logfile", 'f', "LOGFILE", 0,
     "Logfile for result, default /var/log/sysak/tasktop/tasktop.log"},
    {"sort", 's', "SORT", 0,
     "Sort the result, available options are user, sys and cpu, default is "
     "cpu"},
    {"r-limit", 'l', "LIMIT", 0, "Specify the top R-LIMIT tasks to display"},
    {"d-limit", 'e', "D-LIMIT", 0,
     "Specify the D-LIMIT D tasks's stack to display"},
    {"mode", 'm', "MODE", 0, "MODE is load or blocked, default is load"},
    {"threshold", 'b', "TIME(ms)", 0,
     "dtask blocked threshold, default is 3000 ms"},
    {"verbose", 'v', 0, 0, "ebpf program output verbose message"},
    {"run", 'r', "TIME(s)", 0, "run time in secnonds"},
    {"kthread", 'k', 0, 0,
     "blocked-analyze output kernel-thread D stack information"},
    {NULL, 'h', NULL, OPTION_HIDDEN, "Show the full help"},
    {},
};

u64 get_now_ns();

static int prepare_directory(char* path) {
    int ret;

    ret = mkdir(path, 0777);
    if (ret < 0 && errno != EEXIST)
        return errno;
    else
        return 0;
}

/* if out of range or no number found return nonzero */
static int parse_long(const char* str, long* retval) {
    int err = 0;
    char* endptr;
    errno = 0;
    long val = strtol(str, &endptr, 10);

    /* Check for various possible errors */
    if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) ||
        (errno != 0 && val == 0)) {
        fprintf(stderr, "Failed parse val.\n");
        err = errno;
        return err;
    }

    if (endptr == str) return err = -1;
    *retval = val;
    return err;
}

static error_t parse_arg(int key, char* arg, struct argp_state* state) {
    int err = 0;
    long val;
    switch (key) {
        case 'h':
            argp_state_help(state, stderr, ARGP_HELP_STD_HELP);
            break;
        case 'v':
            env.verbose = true;
            break;
        case 't':
            env.thread_mode = true;
            break;
        case 'p':
            err = parse_long(arg, &val);
            if (err) {
                fprintf(stderr, "Failed parse pid.\n");
                argp_usage(state);
            }
            env.tid = val;
            break;
        case 'd':
            err = parse_long(arg, &val);
            if (err || val <= 0) {
                fprintf(stderr, "Failed parse delay.\n");
                argp_usage(state);
            }

            env.delay = val;
            break;
        case 'i':
            err = parse_long(arg, &val);
            if (err || val <= 0) {
                fprintf(stderr, "Failed parse iteration-num.\n");
                argp_usage(state);
            }
            env.nr_iter = val;
            env.nr_iter++;
            break;
        case 'f':
            log_path = arg;
            break;
        case 's':
            if (!strcmp("user", arg)) {
                env.rec_sort = SORT_USER;
            } else if (!strcmp("sys", arg)) {
                env.rec_sort = SORT_SYSTEM;
            } else if (!strcmp("cpu", arg)) {
                env.rec_sort = SORT_CPU;
            } else {
                fprintf(stderr, "Invalid sort type.\n");
                argp_usage(state);
            }
            break;
        case 'l':
            err = parse_long(arg, &val);
            if (err || val <= 0) {
                fprintf(stderr, "Failed parse limit-num.\n");
                argp_usage(state);
            }
            env.limit = val;
            break;
        case 'e':
            err = parse_long(arg, &val);
            if (err || val <= 0) {
                fprintf(stderr, "Failed parse d-stack limit.\n");
                argp_usage(state);
            }
            env.stack_limit = val;
            break;
        case 'H':
            env.human = true;
            break;
        case 'm':
            if (!strcmp(arg, "load")) {
                env.mode = LOAD;
            } else if (!strcmp(arg, "blocked")) {
                env.mode = BLOCKED;
            } else {
                argp_usage(state);
            }
            break;
        case 'b':
            err = parse_long(arg, &val);
            if (err || val <= 0) {
                fprintf(stderr, "Failed parse blocked threshold.\n");
                argp_usage(state);
            }
            env.blocked_ms = val;
            break;
        case 'r':
            err = parse_long(arg, &val);
            if (err || val <= 0) {
                fprintf(stderr, "Failed parse run time.\n");
                argp_usage(state);
            }
            env.run = val;
            break;
        case 'k':
            env.kthread = true;
            break;
        case ARGP_KEY_ARG:
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

static int read_btime() {
    int err = 0;
    char buf[BUF_SIZE];
    long val;
    FILE* fp = fopen(PROC_STAT_PATH, "r");
    if (!fp) {
        fprintf(stderr, "Failed open stat file.\n");
        err = errno;
        goto cleanup;
    }

    while (fgets(buf, BUF_SIZE, fp) != NULL) {
        buf[5] = '\0';
        if (strcmp(buf, "btime") != 0) {
            continue;
        }
        char* str = buf + 6;
        err = parse_long(str, &val);
        if (err) continue;
        btime = val;
        break;
    }
cleanup:
    if (fp) fclose(fp);
    return err;
}

int swap(void* lhs, void* rhs, size_t sz) {
    void* temp = malloc(sz);
    if (!temp) return -1;

    memcpy(temp, lhs, sz);
    memcpy(lhs, rhs, sz);
    memcpy(rhs, temp, sz);

    free(temp);

    return 0;
}

static bool is_D(pid_t pid, pid_t tid, D_task_record_t* t_rec) {
    int res = false;
    char path[FILE_PATH_LEN];

    snprintf(path, FILE_PATH_LEN, "/proc/%d/task/%d/stat", pid, tid);
    FILE* fp = fopen(path, "r");
    if (!fp) {
        return res;
    }

    t_rec->pid = pid;
    memset(t_rec->comm, 0, sizeof(t_rec->comm));
    if (fscanf(fp, "%d %s", &t_rec->tid, t_rec->comm) == EOF) goto cleanup;

    /* process the situation comm contains space,eg. comm=(Signal Dispatch)  */
    while (true) {
        int len = strlen(t_rec->comm);
        if (t_rec->comm[len - 1] == ')') break;
        if (fscanf(fp, "%s", t_rec->comm + len) == EOF) goto cleanup;
    }

    char state;
    if (fscanf(fp, " %c", &state) == EOF) goto cleanup;

    if (state == 'D') res = true;

cleanup:
    fclose(fp);
    return res;
}

static int read_stack(pid_t pid, pid_t tid, D_task_record_t* t_rec) {
#ifdef DEBUG
    fprintf(stderr, "DEBUG: read_stack pid=%d tid=%d\n", pid, tid);
#endif
    int err = 0;
    char stack_path[FILE_PATH_LEN];
    snprintf(stack_path, FILE_PATH_LEN, "/proc/%d/task/%d/stack", pid, tid);
    FILE* fp = fopen(stack_path, "r");
    if (!fp) {
        /* may be thread is exited */
        err = errno;
        goto cleanup;
    }
    memset(t_rec->stack, 0, sizeof(t_rec->stack));
    fread(t_rec->stack, STACK_CONTENT_LEN, 1, fp);

cleanup:
    if (fp) fclose(fp);
    return err;
}

static int read_d_task(struct id_pair_t* pids, int nr_thread, int* stack_num,
                       struct D_task_record_t* d_tasks) {
    if (env.verbose) {
        fprintf(stderr, "DEBUG: read_d_task\n");
    }

    int i = 0, err = 0, d_num = 0;
    for (i = 0; i < nr_thread; i++) {
        if (d_num >= env.stack_limit) break;
        int pid = pids[i].pid;
        int tid = pids[i].tid;

        if (is_D(pid, tid, d_tasks + d_num)) {
            read_stack(pid, tid, d_tasks + d_num);
            d_num++;
        }
    }
    *stack_num = d_num;
    return err;
}

static int read_sched_delay(struct sys_record_t* sys_rec, u64* prev_delay) {
    FILE* fp = fopen(SCHEDSTAT_PATH, "r");
    int err = 0;
    if (!fp) {
        fprintf(stderr, "Failed open stat file.\n");
        err = errno;
        goto cleanup;
    }

    unsigned long long ph;
    unsigned long long delay;
    char name[64];
    char buf[BUF_SIZE];
    while (fscanf(fp, "%s ", name) != EOF) {
        if (!strncmp(name, "cpu", 3)) {
            fscanf(fp, "%llu %llu %llu %llu %llu %llu %llu %llu %llu", &ph, &ph,
                   &ph, &ph, &ph, &ph, &ph, &delay, &ph);

            int cpu_id = atoi(name + 3);

            sys_rec->percpu_sched_delay[cpu_id] = delay - prev_delay[cpu_id];
            prev_delay[cpu_id] = delay;
        } else {
            fgets(buf, BUF_SIZE, fp);
        }
    }

cleanup:
    if (fp) fclose(fp);
    return err;
}

static int check_cgroup(cgroup_cpu_stat_t** prev_cgroups, char* cgroup_name) {
    int i = 0;
    for (i = 0; i < env.cgroup_limit; i++) {
        if (prev_cgroups[i] &&
            !strcmp(cgroup_name, prev_cgroups[i]->cgroup_name)) {
            return i;
        }
    }
    return -1;
}

static cgroup_cpu_stat_t* get_cgroup(cgroup_cpu_stat_t** prev_cgroups,
                                     int idx) {
    int i = 0;
    if (idx < env.cgroup_limit && idx >= 0) {
        return prev_cgroups[idx];
    }

    /* find a empty slot */
    for (i = 0; i < env.cgroup_limit; i++) {
        if (!prev_cgroups[i]) {
            prev_cgroups[i] = calloc(1, sizeof(cgroup_cpu_stat_t));
            prev_cgroups[i]->last_update = time(0);
            return prev_cgroups[i];
        }
    }

    /* find the long time no update slot */
    int last_time = prev_cgroups[0]->last_update;
    int res_idx = 0;
    for (i = 0; i < env.cgroup_limit; i++) {
        if (last_time > prev_cgroups[i]->last_update) {
            last_time = prev_cgroups[i]->last_update;
            res_idx = i;
        }
    }

    return prev_cgroups[res_idx];
}

static int read_cgroup_throttle(cgroup_cpu_stat_t* cgroups, int* cgroup_num,
                                cgroup_cpu_stat_t** prev_cgroups) {
#define CGROUP_PATH "/sys/fs/cgroup/cpu"
    int err = 0;
    struct dirent* dir = 0;
    int c_num = 0;

    DIR* root_dir = opendir(CGROUP_PATH);
    if (!root_dir) {
        fprintf(stderr, "Failed open %s\n", CGROUP_PATH);
        goto cleanup;
    }
    time_t ts = time(0);

    while ((dir = readdir(root_dir)) != NULL) {
        char name[128];
        unsigned long long val = 0;

        if (c_num >= env.cgroup_limit) break;

        if (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..") ||
            dir->d_type != DT_DIR) {
            continue;
        }

        char stat_path[BUF_SIZE];
        snprintf(stat_path, BUF_SIZE, "%s/%s/cpu.stat", CGROUP_PATH,
                 dir->d_name);

        FILE* fp = fopen(stat_path, "r");
        if (!fp) {
            fprintf(stderr, "Failed open cpu.stat[%s].\n", stat_path);
            continue;
        }

        /* if idx == -1, means no history record */
        int idx = check_cgroup(prev_cgroups, dir->d_name);

        /* find a slot, store the history cgroup info */
        cgroup_cpu_stat_t* slot = get_cgroup(prev_cgroups, idx);
        if (!slot) {
            fprintf(stderr, "Get a null cgroup slot.\n");
            err = 1;
            exit(err);
        }

        cgroup_cpu_stat_t* rec = cgroups + c_num;

        memset(slot->cgroup_name, 0, sizeof(slot->cgroup_name));
        memset(rec->cgroup_name, 0, sizeof(rec->cgroup_name));

        strncpy(slot->cgroup_name, dir->d_name, sizeof(slot->cgroup_name));
        strncpy(rec->cgroup_name, dir->d_name, sizeof(rec->cgroup_name));

        while (fscanf(fp, "%s %llu", name, &val) != EOF) {
            if (!strcmp(name, "nr_periods")) {
                if (idx != -1) rec->nr_periods = val - slot->nr_periods;
                slot->nr_periods = val;
            } else if (!strcmp(name, "nr_throttled")) {
                if (idx != -1) rec->nr_throttled = val - slot->nr_throttled;
                slot->nr_throttled = val;
            } else if (!strcmp(name, "throttled_time")) {
                if (idx != -1) rec->throttled_time = val - slot->throttled_time;
                slot->throttled_time = val;
            } else if (!strcmp(name, "nr_burst")) {
                if (idx != -1) rec->nr_burst = val - slot->nr_burst;
                slot->nr_burst = val;
            } else if (!strcmp(name, "burst_time")) {
                if (idx != -1) rec->burst_time = val - slot->burst_time;
                slot->burst_time = val;
            }
        }

        if (rec->nr_throttled > 0 && idx != -1) {
            c_num++;
            slot->last_update = ts;
        }
        fclose(fp);
    }

cleanup:
    if (root_dir) closedir(root_dir);
    *cgroup_num = c_num;
    return err;
}

static int read_stat(struct sys_cputime_t** prev_sys,
                     struct sys_cputime_t** now_sys,
                     struct sys_record_t* sys_rec) {
    int err = 0;
    int i = 0;
    FILE* fp = fopen(PROC_STAT_PATH, "r");
    if (!fp) {
        fprintf(stderr, "Failed open stat file.\n");
        err = errno;
        goto cleanup;
    }

    for (i = 0; i <= nr_cpu; i++) {
        /*now only read first line, maybe future will read more info*/
        fscanf(fp, "%s %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld\n",
               now_sys[i]->cpu, &now_sys[i]->usr, &now_sys[i]->nice,
               &now_sys[i]->sys, &now_sys[i]->idle, &now_sys[i]->iowait,
               &now_sys[i]->irq, &now_sys[i]->softirq, &now_sys[i]->steal,
               &now_sys[i]->guest, &now_sys[i]->guest_nice);

        if (prev_sys[i]->usr == 0) continue;

        int now_time = now_sys[i]->usr + now_sys[i]->sys + now_sys[i]->nice +
                       now_sys[i]->idle + now_sys[i]->iowait + now_sys[i]->irq +
                       now_sys[i]->softirq + now_sys[i]->steal +
                       now_sys[i]->guest + now_sys[i]->guest_nice;
        int prev_time = prev_sys[i]->usr + prev_sys[i]->sys +
                        prev_sys[i]->nice + prev_sys[i]->idle +
                        prev_sys[i]->iowait + prev_sys[i]->irq +
                        prev_sys[i]->softirq + prev_sys[i]->steal +
                        prev_sys[i]->guest + prev_sys[i]->guest_nice;
        int all_time = now_time - prev_time;
        // int all_time = (sysconf(_SC_NPROCESSORS_ONLN) * env.delay *
        // sysconf(_SC_CLK_TCK));

        /* all_time can't calculated by delay * ticks * online-cpu-num,
         * because there is an error between process waked up and running, when
         * sched delay occur , the sum of cpu rates more than 100%. */

#define CALC_SHARE(TIME_TYPE)                                            \
    sys_rec->cpu[i].TIME_TYPE =                                          \
        (double)(now_sys[i]->TIME_TYPE - prev_sys[i]->TIME_TYPE) * 100 / \
        all_time;

        CALC_SHARE(usr)
        CALC_SHARE(nice)
        CALC_SHARE(sys)
        CALC_SHARE(idle)
        CALC_SHARE(iowait)
        CALC_SHARE(irq)
        CALC_SHARE(softirq)
        CALC_SHARE(steal)
        CALC_SHARE(guest)
        CALC_SHARE(guest_nice)
    }
cleanup:
    if (fp) fclose(fp);
    return err;
};

static u64 read_pid_max() {
    int err = 0;
    FILE* fp = fopen(PIDMAX_PATH, "r");
    if (!fp) {
        fprintf(stderr, "Failed read pid max.\n");
        err = errno;
        return err;
    }

    fscanf(fp, "%lu", &pidmax);

    if (fp) fclose(fp);
    return err;
}

static int read_all_pids(struct id_pair_t* pids, u64* num) {
    int err = 0;

    DIR* dir = NULL;
    DIR* task_dir = NULL;
    u64 nr_thread = 0;
    struct dirent* proc_de = NULL;
    struct dirent* task_de = NULL;
    long val;
    pid_t pid, tid;

    dir = opendir("/proc");
    if (!dir) {
        fprintf(stderr, "Failed open %s\n", "/proc");
        err = errno;
        goto cleanup;
    }

    while ((proc_de = readdir(dir)) != NULL) {
        if (proc_de->d_type != DT_DIR || !strcmp(proc_de->d_name, ".") ||
            !strcmp(proc_de->d_name, ".."))
            continue;
        err = parse_long(proc_de->d_name, &val);
        if (err) continue;

        pid = val;
        char taskpath[FILE_PATH_LEN];
        snprintf(taskpath, FILE_PATH_LEN, "/proc/%d/task", pid);
        task_dir = opendir(taskpath);
        if (!task_dir) {
            if (env.verbose) {
                fprintf(stderr, "Failed opendir %s\n", taskpath);
            }

            continue;
        }

        while ((task_de = readdir(task_dir)) != NULL) {
            if (task_de->d_type != DT_DIR || !strcmp(task_de->d_name, ".") ||
                !strcmp(task_de->d_name, ".."))
                continue;
            err = parse_long(task_de->d_name, &val);

            if (err) {
                fprintf(stderr, "Failed parse tid\n");
                break;
            }
            tid = val;

            pids[nr_thread].pid = pid;
            pids[nr_thread++].tid = tid;
        }

        if (task_dir) {
            closedir(task_dir);
            task_dir = NULL;
        }
    }
    *num = nr_thread;
cleanup:
    if (dir) closedir(dir);
    if (task_dir) closedir(task_dir);
    return err;
}

static int read_proc(pid_t pid, pid_t tid, struct task_cputime_t** prev,
                     struct task_cputime_t** now,
                     struct R_task_record_t** rec) {
    struct proc_stat_t proc_info;
    char proc_path[FILE_PATH_LEN];
    struct task_cputime_t* data;
    FILE* fp = 0;
    int err = 0;

    /* tid > 0: tid valid */
    /* tid < 0 0: tid ignored */
    if (tid > 0) {
        snprintf(proc_path, FILE_PATH_LEN, "/proc/%d/task/%d/stat", pid, tid);
        pid = tid;
    } else {
        /* tid < 0 means env is the process mode */
        snprintf(proc_path, FILE_PATH_LEN, "/proc/%d/stat", pid);
    }

    if (!now[pid]) {
        now[pid] = calloc(1, sizeof(struct task_cputime_t));
        if (!now[pid]) {
            fprintf(stderr, "Failed calloc memory.\n");
            err = errno;
            goto cleanup;
        }
    }
    data = now[pid];

    fp = fopen(proc_path, "r");
    if (!fp) {
        /* task maybe exit, this is not error*/
        if (prev[pid]) free(prev[pid]);
        if (now[pid]) free(now[pid]);

        prev[pid] = NULL;
        now[pid] = NULL;
        goto cleanup;
    }

    fscanf(fp, "%d %s", &proc_info.pid, proc_info.comm);

    /* process the situation comm contains space,eg. comm=(Signal Dispatch)  */
    while (true) {
        int len = strlen(proc_info.comm);
        if (proc_info.comm[len - 1] == ')') break;
        if (fscanf(fp, " %s", proc_info.comm + len) == EOF) goto cleanup;
    }

    fscanf(fp,
           " %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu %ld %ld %ld "
           "%ld %ld %ld %lu",
           &proc_info.state, &proc_info.ppid, &proc_info.pgrp,
           &proc_info.session, &proc_info.tty_nr, &proc_info.tpgid,
           &proc_info.flags, &proc_info.minflt, &proc_info.cminflt,
           &proc_info.majflt, &proc_info.cmajflt, &proc_info.utime,
           &proc_info.stime, &proc_info.cutime, &proc_info.cstime,
           &proc_info.priority, &proc_info.nice, &proc_info.num_threads,
           &proc_info.itrealvalue, &proc_info.starttime);

    data->utime = proc_info.utime;
    data->stime = proc_info.stime;
    data->ppid = proc_info.ppid;
    data->starttime = proc_info.starttime;
    data->pid = proc_info.pid;
    data->ts_ns = get_now_ns();

    strcpy(data->comm, proc_info.comm);

    time_t run_time =
        time(0) - btime - (now[pid]->starttime / sysconf(_SC_CLK_TCK));

    if (prev[pid] && !strcmp(prev[pid]->comm, now[pid]->comm)) {
        long udelta = now[pid]->utime - prev[pid]->utime;
        long sdelta = now[pid]->stime - prev[pid]->stime;
        /* if want more accurate, should calculate with clock */
        // long base = env.delay * sysconf(_SC_CLK_TCK);
        long base = (now[pid]->ts_ns - prev[pid]->ts_ns) / NSEC_PER_SECOND *
                    sysconf(_SC_CLK_TCK);

        if (base != 0) {
            /* only process cpu utilization > 0 */
            if (udelta + sdelta > 0) {
                *rec = calloc(1, sizeof(struct R_task_record_t));
                (*rec)->pid = now[pid]->pid;
                (*rec)->ppid = now[pid]->ppid;
                (*rec)->runtime = run_time;
                (*rec)->begin_ts =
                    btime + now[pid]->starttime / sysconf(_SC_CLK_TCK);
                (*rec)->user_cpu_rate = (double)udelta * 100 / base;
                (*rec)->system_cpu_rate = (double)sdelta * 100 / base;
                (*rec)->all_cpu_rate = (double)(udelta + sdelta) * 100 / base;
                strcpy((*rec)->comm, now[pid]->comm);
            }
        }
    }

cleanup:
    if (fp) fclose(fp);
    return err;
}

static void sort_records(struct record_t* rec, int rec_num,
                         enum sort_type sort) {
    struct R_task_record_t** records = rec->r_tasks;
    int i, j;
    for (i = 0; i < rec_num; i++) {
        for (j = i + 1; j < rec_num; j++) {
            if (!records[j] && !records[i]) {
                continue;
            } else if (records[i] && !records[j]) {
                continue;
            } else if (!records[i] && records[j]) {
                swap(&records[i], &records[j], sizeof(struct R_task_record_t*));
            } else {
                double lth, rth;
                switch (sort) {
                    case SORT_SYSTEM:
                        lth = records[i]->system_cpu_rate;
                        rth = records[j]->system_cpu_rate;
                        break;
                    case SORT_USER:
                        lth = records[i]->user_cpu_rate;
                        rth = records[j]->user_cpu_rate;
                        break;
                    case SORT_CPU:
                        lth = records[i]->all_cpu_rate;
                        rth = records[j]->all_cpu_rate;
                        break;
                    default:
                        fprintf(stderr, "Unknown SORT_TYPE\n");
                        return;
                }

                if (lth < rth) {
                    swap(&records[i], &records[j],
                         sizeof(struct R_task_record_t*));
                }
            }
        }
    }
}

static char* ts2str(time_t ts, char* buf, int size) {
    struct tm* t = gmtime(&ts);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", t);
    return buf;
}

static void build_str(int day, int hour, int min, int sec, char* buf) {
    char tmp[32];
    if (day > 0) {
        snprintf(tmp, 32, "%dd,", day);
        strcat(buf, tmp);
    }
    if (hour > 0) {
        snprintf(tmp, 32, "%dh,", hour);
        strcat(buf, tmp);
    }
    if (min > 0) {
        snprintf(tmp, 32, "%dm,", min);
        strcat(buf, tmp);
    }
    if (sec > 0) {
        snprintf(tmp, 32, "%ds", sec);
        strcat(buf, tmp);
    }
}

static char* second2str(time_t ts, char* buf, int size) {
#define MINUTE 60
#define HOUR (MINUTE * 60)
#define DAY (HOUR * 24)
    int day = (int)ts / DAY;
    ts = ts % DAY;
    int hour = (int)ts / HOUR;
    ts = ts % HOUR;
    int minute = (int)ts / MINUTE;
    ts = ts % MINUTE;

    memset(buf, 0, size);
    build_str(day, hour, minute, ts, buf);
    return buf;
}

static void output_ts(FILE* dest) {
    char stime_str[BUF_SIZE] = {0};
    time_t now = time(0);
    fprintf(dest, "[TIME-STAMP] %s\n", ts2str(now, stime_str, BUF_SIZE));
}

static void output_sys_load(struct record_t* rec, FILE* dest) {
    struct sys_record_t* sys = &rec->sys;
    struct proc_fork_info_t* info = &(sys->most_fork_info);
    fprintf(dest, "[UTIL&LOAD]\n");
    fprintf(dest, "%6s %6s %6s %6s %6s %6s %6s :%5s \n", "usr", "sys", "iowait",
            "load1", "R", "D", "fork", "proc");

    fprintf(dest, "%6.1f %6.1f %6.1f %6.1f %6d %6d %6d", sys->cpu[0].usr,
            sys->cpu[0].sys, sys->cpu[0].iowait, sys->load1, sys->nr_R,
            sys->nr_D, sys->nr_fork);
    fprintf(dest, " : %s(%d) ppid=%d cnt=%lu \n", info->comm, info->pid,
            info->ppid, info->fork);
}

static void output_per_cpu(struct record_t* rec, FILE* dest) {
    int i;
    struct sys_record_t* sys = &rec->sys;

    //  18.8 us, 14.1 sy,  0.0 ni, 65.6 id,  0.0 wa,  0.0 hi,  1.6 si,  0.0 st
    fprintf(dest, "[PER-CPU]\n");
    fprintf(dest, "%7s %6s %6s %6s %6s %6s %6s %6s %6s %10s\n", "cpu", "usr",
            "sys", "nice", "idle", "iowait", "h-irq", "s-irq", "steal",
            "delay(ms)");
    for (i = 1; i <= nr_cpu; i++) {
        char cpu_name[16];
        snprintf(cpu_name, 16, "cpu-%d", i - 1);
        fprintf(dest,
                "%7s %6.1f %6.1f %6.1f %6.1f %6.1f %6.1f %6.1f %6.1f %10lu\n",
                cpu_name, sys->cpu[i].usr, sys->cpu[i].sys, sys->cpu[i].nice,
                sys->cpu[i].idle, sys->cpu[i].iowait, sys->cpu[i].irq,
                sys->cpu[i].softirq, sys->cpu[i].steal,
                sys->percpu_sched_delay[i - 1] / (1000 * 1000));
    }
}

static void output_cgroup(struct record_t* rec, int cgroup_num, FILE* dest) {
    cgroup_cpu_stat_t* cgroups = rec->cgroups;

    int i = 0;

    for (i = 0; i < cgroup_num; i++) {
        if (i == 0) {
            fprintf(dest, "[CGROUP]\n");
            fprintf(dest, "%20s %15s %15s %15s %15s %15s\n", "cgroup_name",
                    "nr_periods", "nr_throttled", "throttled_time", "nr_burst",
                    "burst_time");
        }
        fprintf(dest, "%20s %15d %15d %15lu %15d %15lu\n",
                cgroups[i].cgroup_name, cgroups[i].nr_periods,
                cgroups[i].nr_throttled, cgroups[i].throttled_time,
                cgroups[i].nr_burst, cgroups[i].burst_time);
    }
}

static void output_tasktop(struct record_t* rec, int rec_num, FILE* dest) {
    struct R_task_record_t** records = rec->r_tasks;
    int i;
    char rtime_str[BUF_SIZE] = {0};
    char stime_str[BUF_SIZE] = {0};

    for (i = 0; i < rec_num; i++) {
        if (!records[i]) break;

        if (env.human) {
            if (i == 0) {
                fprintf(dest, "[TASKTOP]\n");
                fprintf(dest, "%18s %6s %6s %20s %15s %6s %6s %6s\n", "COMMAND",
                        "PID", "PPID", "START", "RUN", "%UTIME", "%STIME",
                        "%CPU");
            }

            if (i >= env.limit) break;
            fprintf(dest, "%18s %6d %6d %20s %15s %6.1f %6.1f %6.1f\n",
                    records[i]->comm, records[i]->pid, records[i]->ppid,
                    ts2str(records[i]->begin_ts, stime_str, BUF_SIZE),
                    second2str(records[i]->runtime, rtime_str, BUF_SIZE),
                    records[i]->user_cpu_rate, records[i]->system_cpu_rate,
                    records[i]->all_cpu_rate);
        } else {
            if (i == 0) {
                fprintf(dest, "[TASKTOP]\n");
                fprintf(dest, "%18s %6s %6s %10s %10s %6s %6s %6s\n", "COMMAND",
                        "PID", "PPID", "START", "RUN", "%UTIME", "%STIME",
                        "%CPU");
            }

            if (i >= env.limit) break;
            fprintf(dest, "%18s %6d %6d %10ld %10ld %6.1f %6.1f %6.1f\n",
                    records[i]->comm, records[i]->pid, records[i]->ppid,
                    records[i]->begin_ts, records[i]->runtime,
                    records[i]->user_cpu_rate, records[i]->system_cpu_rate,
                    records[i]->all_cpu_rate);
        }
    }
}

static void output_stack_with_offset(int off, char* str, FILE* dest) {
    const char delim[2] = "\n";
    char* token;

    token = strtok(str, delim);
    fprintf(dest, "%s\n", token);

    while (true) {
        token = strtok(NULL, delim);
        if (!token) break;
        int cnt = 0;
        while (cnt < off) {
            fprintf(dest, " ");
            cnt++;
        }
        fprintf(dest, "%s\n", token);
    }
}

static void output_d_stack(struct record_t* rec, int d_num, FILE* dest) {
    int i;
    struct D_task_record_t* d_tasks = rec->d_tasks;
    char* str = calloc(STACK_CONTENT_LEN, sizeof(char));
    for (i = 0; i < d_num; i++) {
        if (i == 0) {
            fprintf(dest, "[D-STASK]\n");
            fprintf(dest, "%18s %6s %6s %6s\n", "COMMAND", "PID", "PPID",
                    "STACK");
        }
        fprintf(dest, "%18s %6d %6d ", d_tasks[i].comm, d_tasks[i].tid,
                d_tasks[i].pid);

        strncpy(str, d_tasks[i].stack, STACK_CONTENT_LEN - 1);

        output_stack_with_offset(18 + 6 + 6 + 3, str, dest);
    }

    free(str);
}

static bool inline is_high_load1(struct record_t* rec) {
#define THRESHOLD_LOAD1 nr_cpu * 1.5
    return rec->sys.load1 >= THRESHOLD_LOAD1;
}

static bool inline is_high_R(struct record_t* rec) {
#define THRESHOLD_R nr_cpu
    return rec->sys.nr_R >= THRESHOLD_R;
}

static bool inline is_high_D(struct record_t* rec) {
#define THRESHOLD_D 8
    return rec->sys.nr_D >= THRESHOLD_D;
}

double inline calculate_sys(cpu_util_t* cpu) {
    double sys_util = cpu->iowait + cpu->sys + cpu->softirq + cpu->irq;
    return sys_util;
}

double inline calculate_overall(cpu_util_t* cpu) {
    double overall_cpuutil = cpu->iowait + cpu->sys + cpu->softirq + cpu->irq +
                             +cpu->nice + cpu->usr;
    return overall_cpuutil;
}

static void inline is_high_overall_cpuutil(struct record_t* rec, FILE* dest) {
#define THRESHOLD_CPU_OVERLOAD 55
    cpu_util_t* cpu = rec->sys.cpu;
    double overall_cpuutil = calculate_overall(cpu);
    if (overall_cpuutil >= THRESHOLD_CPU_OVERLOAD) {
        fprintf(dest, "WARN: CPU overall utilization is high.\n");
    }
}

static void inline is_high_sys(struct record_t* rec, FILE* dest) {
#define THRESHOLD_SYS 15
    double sys_util = calculate_sys(rec->sys.cpu);
    if (sys_util >= THRESHOLD_SYS) {
        fprintf(dest, "INFO: Sys time of cpu is high.\n");
    }
}

static void is_bind(struct record_t* rec, FILE* dest) {
#define THRESHOLD_BIND 25
    int i;
    cpu_util_t* cpu = rec->sys.cpu;
    double min_overall = calculate_overall(cpu);

    for (i = 1; i <= nr_cpu; i++) {
        double overall = calculate_overall(cpu + i);
        if (overall < min_overall) min_overall = overall;
    }

    bool status[nr_cpu];
    bool exist = false;
    for (i = 1; i <= nr_cpu; i++) {
        double overall = calculate_overall(cpu + i);
        if (overall - min_overall > THRESHOLD_BIND) {
            status[i - 1] = true;
            exist = true;
        } else {
            status[i - 1] = false;
        }
    }

    if (exist) {
        fprintf(dest, "WARN: Some tasks bind cpu. Please check cpu: ");
        for (i = 1; i <= nr_cpu; i++) {
            if (status[i - 1]) {
                fprintf(dest, " [%d]", i - 1);
            }
        }
        fprintf(dest, "\n");
    }
}

static void group_by_stack(struct record_t* rec, FILE* dest, int d_num) {
#define PREFIX_LEN 64
    D_task_record_t* dtask = rec->d_tasks;
    int* counter = calloc(d_num, sizeof(int));
    D_task_record_t** stack = calloc(d_num, sizeof(D_task_record_t*));
    if (!counter || !stack) {
        fprintf(stderr, "Failed calloc counter and stack.\n");
        exit(1);
    }

    int empty_slot = 0;
    int i, j;
    for (i = 0; i < d_num; i++) {
        D_task_record_t* s = dtask + i;
        bool match = false;
        for (j = 0; j < empty_slot; j++) {
            if (stack[j]) {
                if (!strncmp(stack[j]->stack, s->stack, 64)) {
                    counter[j]++;
                    match = true;
                    break;
                }
            }
        }

        if (!match) {
            stack[empty_slot] = s;
            counter[empty_slot++] = 1;
        }
    }

    int max_idx = -1;
    int max_times = -1;
    for (i = 0; i < empty_slot; i++) {
        if (counter[i] > max_times) {
            max_times = counter[i];
            max_idx = i;
        }
    }

    /* maybe there is no d-stack */
    if (max_idx != -1) {
        fprintf(dest, "WARN: The most stack, times=%d\n", max_times);
        fprintf(dest, "%s", stack[max_idx]->stack);
    }

    free(stack);
    free(counter);
}

static void load_analyse(struct record_t* rec, int rec_num, FILE* dest,
                         int d_num, int cgroup_num) {
    fprintf(dest, "[EXCEPTION&ADVICE]\n");

    if (is_high_load1(rec)) {
        fprintf(dest, "INFO: Load is abnormal.\n");
        if (is_high_R(rec)) {
            is_high_overall_cpuutil(rec, dest);

            is_high_sys(rec, dest);

            is_bind(rec, dest);
        }

        if (is_high_D(rec)) {
            group_by_stack(rec, dest, d_num);
        }
    } else {
        fprintf(dest, "INFO: Load is normal.\n");
    }
}

static void output(struct record_t* rec, int rec_num, FILE* dest, int d_num,
                   int cgroup_num) {
    output_ts(dest);
    output_sys_load(rec, dest);
    output_per_cpu(rec, dest);
    output_cgroup(rec, cgroup_num, dest);
    output_tasktop(rec, rec_num, dest);
    output_d_stack(rec, d_num, dest);

    load_analyse(rec, rec_num, dest, d_num, cgroup_num);
    fflush(dest);
}

static void now_to_prev(struct id_pair_t* pids, int nr_thread, int pidmax,
                        struct task_cputime_t** prev_task,
                        struct task_cputime_t** now_task,
                        struct sys_cputime_t** prev_sys,
                        struct sys_cputime_t** now_sys) {
    int i;
    for (i = 0; i < pidmax; i++) {
        if (prev_task[i]) {
            free(prev_task[i]);
            prev_task[i] = NULL;
        }
    }

    for (i = 0; i < nr_thread; i++) {
        int pid;
        if (env.thread_mode)
            pid = pids[i].tid;
        else {
            /* only move once */
            if (pids[i].pid != pids[i].tid) continue;
            pid = pids[i].pid;
        }

        swap(&prev_task[pid], &now_task[pid], sizeof(struct task_cputime_t*));
    }

    for (i = 0; i <= nr_cpu; i++) {
        swap(&prev_sys[i], &now_sys[i], sizeof(struct sys_cputime_t*));
    }
}

static int make_records(struct id_pair_t* pids, int nr_thread,
                        struct record_t* rec, struct task_cputime_t** prev_task,
                        struct task_cputime_t** now_task, int* rec_num) {
    struct R_task_record_t** records = rec->r_tasks;
    int err = 0;
    u64 i;
    int nr_rec = 0;

    for (i = 0; i < nr_thread; i++) {
        struct id_pair_t* id = &pids[i];

        if (env.tid != -1) {
            if (env.thread_mode) {
                if (id->tid != env.tid) continue;
            } else {
                if (id->pid != env.tid) continue;
            }
        }

        /* many pair with the same pid, in process mode skip the trival read
         */
        if (!env.thread_mode && id->pid != id->tid) continue;

        if (env.thread_mode) {
            err = read_proc(id->pid, id->tid, prev_task, now_task,
                            &records[nr_rec++]);
        } else {
            err =
                read_proc(id->pid, -1, prev_task, now_task, &records[nr_rec++]);
        }

        if (err) {
            fprintf(stderr, "Failed read proc\n");
            return err;
        }
    }
    *rec_num = nr_rec;

    return err;
}

static void free_records(struct record_t* rec, int nr_thread) {
    R_task_record_t** records = rec->r_tasks;
    int i;
    for (i = 0; i < nr_thread; i++) {
        if (records[i]) free(records[i]);
    }
    free(records);
}

static FILE* open_logfile() {
    FILE* stat_log = 0;
    if (!log_path) {
        log_path = default_log_path;
    }

    stat_log = fopen(log_path, "w");

    return stat_log;
}

static int libbpf_print_fn(enum libbpf_print_level level, const char* format,
                           va_list args) {
    if (level == LIBBPF_DEBUG && !env.verbose) return 0;
    return vfprintf(stderr, format, args);
}

static int bump_memlock_rlimit(void) {
    struct rlimit rlim_new = {
        .rlim_cur = RLIM_INFINITY,
        .rlim_max = RLIM_INFINITY,
    };

    return setrlimit(RLIMIT_MEMLOCK, &rlim_new);
}

static int check_fork(int fork_map_fd, struct sys_record_t* sys_rec) {
    int fd;
    int err;
    u64 total = 0;
    u64 lookup_key = -1, next_key;
    struct proc_fork_info_t info;

    fd = fork_map_fd;
    int max_fork = 0;
    while (!bpf_map_get_next_key(fd, &lookup_key, &next_key)) {
        err = bpf_map_lookup_elem(fd, &next_key, &info);

        err = bpf_map_delete_elem(fd, &next_key);
        if (err < 0) {
            fprintf(stderr, "Failed to delete elem: %d\n", err);
            return -1;
        }
        lookup_key = next_key;

        if (!next_key) continue;

        total = total + info.fork;

        if (max_fork < info.fork) {
            max_fork = info.fork;
            sys_rec->most_fork_info = info;
        }
    }
    sys_rec->nr_fork = total;
    return err;
}

static void sigint_handler(int signo) { exiting = 1; }

void handle_lost_events(void* ctx, int cpu, __u64 lost_cnt) {
    fprintf(stderr, "Lost %llu events on CPU #%d!\n", lost_cnt, cpu);
}

void handle_event(void* ctx, int cpu, void* data, __u32 data_sz) {
    const struct d_task_blocked_event_t* ev = data;
    char stime_str[BUF_SIZE] = {0};
    char* tstr = ts2str(time(0), stime_str, BUF_SIZE);

    fprintf(env.dest, "%20s %10s %16s %10d %20lu %lu\n", tstr, "Stop-D",
            ev->info.comm, ev->pid, ev->start_time_ns,
            ev->duration_ns / NSEC_PER_MILLSECOND);
}

u64 get_now_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * NSEC_PER_SECOND + ts.tv_nsec;
}

u64 check_d_task_timeout(int d_task_map_fd, u64 threshold_ns, FILE* dest) {
    struct d_task_info_t info;
    u64 now_ns = 0, min_start_time_ns = UINT64_MAX;
    int err = 0;
    struct d_task_key_t lookup_key = {.pid = 0, .start_time_ns = 0};
    struct d_task_key_t next_key;
    char stime_str[BUF_SIZE] = {0};
    char* tstr = ts2str(time(0), stime_str, BUF_SIZE);

    char* str = calloc(STACK_CONTENT_LEN, sizeof(char));

    /* check all d task */
    while (!bpf_map_get_next_key(d_task_map_fd, &lookup_key, &next_key)) {
        err = bpf_map_lookup_elem(d_task_map_fd, &next_key, &info);
        if (err < 0) {
            // this d task maybe deleted by eBPF
            goto check_next_key;
        }

        if (info.is_recorded == 0) {
            /* must get now again, avoid time resolution problem*/
            now_ns = get_now_ns();
            if ((now_ns - next_key.start_time_ns) >= threshold_ns) {
                D_task_record_t st;
                err = read_stack(next_key.pid, next_key.pid, &st);
                if (err || strlen(st.stack) == 0) {
                    goto check_next_key;
                }

                /* if stack get caught the mark the task first */
                info.is_recorded = 1;
                err = bpf_map_update_elem(d_task_map_fd, &next_key, &info,
                                          BPF_EXIST);
                if (err) {
                    goto check_next_key;
                }

                strncpy(str, st.stack, STACK_CONTENT_LEN - 1);
                fprintf(dest, "%20s %10s %16s %10d %20lu ", tstr, "Timeout",
                        info.comm, next_key.pid, next_key.start_time_ns);

                output_stack_with_offset(20 + 10 + 16 + 10 + 20 + 5, str, dest);
            } else {
                min_start_time_ns = next_key.start_time_ns < min_start_time_ns
                                        ? next_key.start_time_ns
                                        : min_start_time_ns;
            }
        }
    check_next_key:
        lookup_key = next_key;
    }
    free(str);
    return min_start_time_ns;
}

void wait_d_task_timeout(u64 threshold_ns, u64 min_start_time_ns) {
    /* default sleep  threshold_ns, if there is a task will timeout, no need
     * sleep threshold_ns*/
    u64 now_ns = get_now_ns();

    /* reset err to return 0 if exiting */
    u64 sleep_ns = threshold_ns;
    struct timespec ts, rem;
    int err = 0;
    /* sleep some time */
    if (min_start_time_ns != UINT64_MAX) {
        u64 wait_time_ns = min_start_time_ns + threshold_ns > now_ns
                               ? min_start_time_ns + threshold_ns - now_ns
                               : 0;
        sleep_ns = wait_time_ns;
    }

    ts.tv_sec = sleep_ns / NSEC_PER_SECOND;
    ts.tv_nsec = sleep_ns % NSEC_PER_SECOND;

wait_sleep:
    err = nanosleep(&ts, &rem);
    /* if interupt by signal, but not SIGINT contine sleep */
    if (err != 0 && !exiting) {
        ts = rem;
        goto wait_sleep;
    }
    return;
}

struct tasktop_state {
    struct id_pair_t* pids;
    struct task_cputime_t** prev_task;
    struct task_cputime_t** now_task;
    struct cgroup_cpu_stat_t** prev_cgroup;
    struct sys_cputime_t** prev_sys;
    struct sys_cputime_t** now_sys;
    struct record_t* rec;
    u64* prev_delay;
} tasktop_state = {.pids = 0,
                   .prev_task = 0,
                   .now_task = 0,
                   .prev_cgroup = 0,
                   .prev_sys = 0,
                   .now_sys = 0,
                   .rec = 0,
                   .prev_delay = 0};

int init_state() {
    int i = 0;
    tasktop_state.rec = calloc(1, sizeof(struct record_t));
    tasktop_state.rec->sys.cpu = calloc(nr_cpu + 1, sizeof(struct cpu_util_t));
    tasktop_state.rec->sys.percpu_sched_delay = calloc(nr_cpu, sizeof(u64));
    tasktop_state.rec->d_tasks =
        calloc(env.stack_limit, sizeof(struct D_task_record_t));
    tasktop_state.rec->cgroups =
        calloc(env.cgroup_limit, sizeof(cgroup_cpu_stat_t));

    if (!tasktop_state.rec || !tasktop_state.rec->sys.cpu ||
        !tasktop_state.rec->sys.percpu_sched_delay ||
        !tasktop_state.rec->d_tasks || !tasktop_state.rec->cgroups) {
        fprintf(stderr, "Failed calloc memory\n");
        return -1;
    }

    tasktop_state.prev_cgroup =
        calloc(env.cgroup_limit, sizeof(struct cgroup_cpu_stat_t*));
    tasktop_state.prev_delay = calloc(nr_cpu, sizeof(u64));
    tasktop_state.pids = calloc(pidmax + 1, sizeof(struct id_pair_t));
    tasktop_state.prev_task =
        calloc(pidmax + 1, sizeof(struct task_cputime_t*));
    tasktop_state.now_task = calloc(pidmax + 1, sizeof(struct task_cputime_t*));
    tasktop_state.prev_sys = calloc(1 + nr_cpu, sizeof(struct sys_cputime_t*));
    tasktop_state.now_sys = calloc(1 + nr_cpu, sizeof(struct sys_cputime_t*));

    for (i = 0; i <= nr_cpu; i++) {
        tasktop_state.prev_sys[i] = calloc(1, sizeof(struct sys_cputime_t));
        tasktop_state.now_sys[i] = calloc(1, sizeof(struct sys_cputime_t));
    }
    tasktop_state.prev_delay = calloc(nr_cpu, sizeof(u64));
    if (!tasktop_state.prev_task || !tasktop_state.now_task ||
        !tasktop_state.prev_delay || !tasktop_state.pids ||
        !tasktop_state.prev_sys || !tasktop_state.now_sys ||
        !tasktop_state.prev_cgroup) {
        fprintf(stderr, "Failed calloc memory.\n");
        return -1;
    }
    return 0;
}

void destory_state() {
    int i = 0;

    if (tasktop_state.rec) {
        free(tasktop_state.rec->cgroups);
        free(tasktop_state.rec->d_tasks);
        free(tasktop_state.rec->sys.percpu_sched_delay);
        free(tasktop_state.rec->sys.cpu);
        free(tasktop_state.rec);
    }

    if (tasktop_state.now_sys && tasktop_state.prev_sys) {
        for (i = 0; i <= nr_cpu; i++) {
            free(tasktop_state.prev_sys[i]);
            free(tasktop_state.now_sys[i]);
        }

        free(tasktop_state.now_sys);
        free(tasktop_state.prev_sys);
    }

    if (tasktop_state.now_task) free(tasktop_state.now_task);
    if (tasktop_state.prev_task) free(tasktop_state.prev_task);
    if (tasktop_state.pids) free(tasktop_state.pids);
    if (tasktop_state.prev_delay) free(tasktop_state.prev_delay);
    if (tasktop_state.prev_cgroup) free(tasktop_state.prev_cgroup);
}

int main(int argc, char** argv) {
    int err = 0, fork_map_fd = -1, d_task_map_fd = -1,
        d_task_notify_map_fd = -1, arg_map_fd = -1;
    FILE* stat_log = 0;
    struct tasktop_bpf* skel = 0;

    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        fprintf(stderr, "Failed set signal handler.\n");
        return -errno;
    }

    /* parse args */
    static const struct argp argp = {
        .options = opts,
        .parser = parse_arg,
        .doc = argp_program_doc,
    };

    err = argp_parse(&argp, argc, argv, 0, 0, 0);
    if (err) {
        fprintf(stderr, "Failed parse args.\n");
        goto cleanup;
    }

    libbpf_set_print(libbpf_print_fn);
    bump_memlock_rlimit();

    /* prepare the logfile */
    prepare_directory(log_dir);
    /* prepare ebpf */
    env.dest = open_logfile();
    if (!env.dest) {
        fprintf(stderr, "Failed open stat log file.\n");
        goto cleanup;
    }

    skel = tasktop_bpf__open();
    if (!skel) {
        err = 1;
        fprintf(stderr, "Failed to open BPF skeleton.\n");
        goto cleanup;
    }

    err = tasktop_bpf__load(skel);
    if (err) {
        fprintf(stderr, "Failed to load BPF skeleton.\n");
        goto cleanup;
    }

    fork_map_fd = bpf_map__fd(skel->maps.fork_map);
    d_task_map_fd = bpf_map__fd(skel->maps.d_task_map);
    d_task_notify_map_fd = bpf_map__fd(skel->maps.d_task_notify_map);
    arg_map_fd = bpf_map__fd(skel->maps.arg_map);

    err = tasktop_bpf__attach(skel);
    if (err) {
        fprintf(stderr, "Failed to attach BPF skeleton.\n");
        goto cleanup;
    }

    /* send argument to kernel space */
    struct arg_to_bpf arg = {
        .fork_enable = env.mode == LOAD ? 1 : 0,
        .blocked_enable = env.mode == BLOCKED ? 1 : 0,
        .verbose_enable = env.verbose ? 1 : 0,
        .kthread_enable = env.kthread ? 1 : 0,
        .threshold_ns = env.blocked_ms * NSEC_PER_MILLSECOND};
    int key = 0;
    bpf_map_update_elem(arg_map_fd, &key, &arg, BPF_ANY);

    if (env.mode == LOAD) {
        /* prepare load analyse */
        nr_cpu = sysconf(_SC_NPROCESSORS_ONLN);
        /* init pid_max and btime */
        err = read_pid_max();
        if (err) {
            fprintf(stderr, "Failed read pid max.\n");
            goto cleanup;
        }

        err = read_btime();
        if (err) {
            fprintf(stderr, "Failed read btime.\n");
            goto cleanup;
        }

        err = init_state();
        if (err) {
            fprintf(stderr, "Failed init state.\n");
            goto cleanup;
        }

        bool first = true;
        while (env.nr_iter-- && !exiting) {
            u64 nr_thread = 0;
            int rec_num = 0;
            int d_num = 0;
            int cgroup_num = 0;

            read_cgroup_throttle(tasktop_state.rec->cgroups, &cgroup_num,
                                 tasktop_state.prev_cgroup);
            read_sched_delay(&tasktop_state.rec->sys, tasktop_state.prev_delay);
            check_fork(fork_map_fd, &tasktop_state.rec->sys);
            runnable_proc(&tasktop_state.rec->sys);
            unint_proc(&tasktop_state.rec->sys);
            read_stat(tasktop_state.prev_sys, tasktop_state.now_sys,
                      &tasktop_state.rec->sys);
            /* get all process now */
            read_all_pids(tasktop_state.pids, &nr_thread);
            read_d_task(tasktop_state.pids, nr_thread, &d_num,
                        tasktop_state.rec->d_tasks);
            /* onlu alloc a array, the taskinfo allco in make records */
            tasktop_state.rec->r_tasks =
                calloc(nr_thread, sizeof(struct R_task_record_t*));
            /* if prev process info exist produce record*/
            err = make_records(tasktop_state.pids, nr_thread, tasktop_state.rec,
                               tasktop_state.prev_task, tasktop_state.now_task,
                               &rec_num);
            if (err) {
                fprintf(stderr, "Failed make records.\n");
                goto cleanup;
            }

            /* sort record by sort type */
            sort_records(tasktop_state.rec, rec_num, env.rec_sort);
            /* output record */
            if (!first)
                output(tasktop_state.rec, rec_num, env.dest, d_num, cgroup_num);
            else
                first = false;
            free_records(tasktop_state.rec, nr_thread);
            /* update old info and free nonexist process info */
            now_to_prev(tasktop_state.pids, nr_thread, pidmax,
                        tasktop_state.prev_task, tasktop_state.now_task,
                        tasktop_state.prev_sys, tasktop_state.now_sys);

            if (env.nr_iter) {
                sleep(env.delay);
            }
        }
    } else if (env.mode == BLOCKED) {
        int err = 0;
        uint64_t min_start_time_ns = UINT64_MAX,
                 threshold_ns = env.blocked_ms * NSEC_PER_MILLSECOND;
        struct perf_buffer_opts pb_opts = {.sample_cb = handle_event,
                                           .lost_cb = handle_lost_events};
        struct perf_buffer* pb =
            perf_buffer__new(d_task_notify_map_fd, 8, &pb_opts);
        if (libbpf_get_error(pb)) {
            fprintf(stderr, "Failed to create perf buffer.\n");
            goto cleanup;
        }

        fprintf(env.dest, "%20s %10s %16s %10s %20s %s\n", "Time", "Event",
                "Comm", "Pid", "Start(ns)", "Stack|Delya(ms)");
        time_t run_start_s = time(0);

        while (!exiting) {
            if (env.run != -1 && time(0) - run_start_s > env.run) {
                break;
            }

            min_start_time_ns =
                check_d_task_timeout(d_task_map_fd, threshold_ns, env.dest);

            /* process d task become running*/
            err = perf_buffer__poll(pb, 0);
            if (err < 0 && err != -EINTR) {
                fprintf(stderr, "error polling perf buffer: %s\n",
                        strerror(-err));
                goto cleanup;
            }

            wait_d_task_timeout(threshold_ns, min_start_time_ns);
            fflush(env.dest);
        }
    }
cleanup:
    destory_state();
    if (stat_log) fclose(stat_log);
    tasktop_bpf__destroy(skel);
    return err;
}
