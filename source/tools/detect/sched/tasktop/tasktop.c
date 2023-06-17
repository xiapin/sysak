#include <argp.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <stdbool.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h> /* bpf_obj_pin */
#include "bpf/tasktop.skel.h"
#include "procstate.h"
#include "tasktop.h"
#include "common.h"

// #define DEBUG
// #define LOG_DEBUG
// #define ONLY_THREAD
// #define STRESS_TEST

char log_dir[FILE_PATH_LEN] = "/var/log/sysak/tasktop";
char default_log_path[FILE_PATH_LEN] = "/var/log/sysak/tasktop/tasktop.log";
time_t btime = 0;
u_int64_t pidmax = 0;
char* log_path = 0;
int nr_cpu;
unsigned long long* prev_delay;
static volatile sig_atomic_t exiting;

struct env {
    bool thread_mode;
    time_t delay;
    pid_t tid;
    long nr_iter;
    enum sort_type rec_sort;
    int limit;
    bool human;
    int stack_limit;
    int cgroup_limit;
} env = {.thread_mode = false,
         .delay = 3,
         .tid = -1,
         .human = false,
         .rec_sort = SORT_CPU,
         .nr_iter = LONG_MAX - 1,
         .limit = INT_MAX,
         .stack_limit = 20,
         .cgroup_limit = 20};

const char* argp_program_version = "tasktop 0.1";
const char argp_program_doc[] =
    "A light top, display the process/thread cpu utilization in peroid.\n"
    "\n"

    "USAGE: tasktop [--help] [-t] [-p TID] [-d DELAY] [-i ITERATION] [-s SORT] "
    "[-f LOGFILE] [-l "
    "LIMIT]\n"
    "\n"

    "EXAMPLES:\n"
    "    tasktop            # run forever, display the cpu utilization.\n"
    "    tasktop -t         # display all thread.\n"
    "    tasktop -p 1100    # only display task with pid 1100.\n"
    "    tasktop -d 5       # modify the sample interval.\n"
    "    tasktop -i 3       # output 3 times then exit.\n"
    "    tasktop -l 20      # limit the records number no more than 20.\n"
    "    tasktop -e 10      # limit the d-stack no more than 10, default is "
    "20.\n"
    "    tasktop -f a.log   # log to a.log \n";

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
    {"d-limit", 'e', "STACK-LIMIT", 0,
     "Specify the STACK-LIMIT D tasks's stack to display"},

    {NULL, 'h', NULL, OPTION_HIDDEN, "Show the full help"},
    {},
};

/* PROCESS MODE
 /proc/pid/stat -- calculate process cpu util
 /proc/pid/task/tid/stat -- check task state, if d read the stack */

/* THREAD MODE
/proc/pid/task/tid/stat -- calcualte thread cpu util
/proc/pid/task/tid/sat -- read task state, if d read stack
*/
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
        };
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
#ifdef DEBUG
    fprintf(stderr, "DEBUG: read_d_task\n");
#endif
    int i = 0;
    int err = 0;

#ifdef DEBUG
    struct timeval start, end;
    err = gettimeofday(&start, 0);
    if (err) fprintf(stderr, "read start time error.\n");
#endif

    int d_num = 0;
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

#ifdef DEBUG
    err = gettimeofday(&end, 0);
    if (err) fprintf(stderr, "read end time error.\n");
    fprintf(stderr, "read %d thread user %lds %ldus.\n", nr_thread,
            end.tv_sec - start.tv_sec, end.tv_usec - start.tv_usec);
#endif

    return err;
}

static int read_sched_delay(struct sys_record_t* sys_rec) {
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
    return 0;
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

        /* if idx == -1,means no history record */
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

        strncpy(slot->cgroup_name, dir->d_name, sizeof(slot->cgroup_name) - 1);
        strncpy(rec->cgroup_name, dir->d_name, sizeof(rec->cgroup_name) - 1);

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

        sys_rec->cpu[i].usr =
            (double)(now_sys[i]->usr - prev_sys[i]->usr) * 100 / all_time;
        sys_rec->cpu[i].sys =
            (double)(now_sys[i]->sys - prev_sys[i]->sys) * 100 / all_time;
        sys_rec->cpu[i].iowait =
            (double)(now_sys[i]->iowait - prev_sys[i]->iowait) * 100 / all_time;
    }
cleanup:
    if (fp) fclose(fp);
    return err;
};

static u_int64_t read_pid_max() {
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

static int read_all_pids(struct id_pair_t* pids, u_int64_t* num) {
    int err = 0;

    DIR* dir = NULL;
    DIR* task_dir = NULL;
    u_int64_t nr_thread = 0;
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
            // fprintf(stderr, "Failed opendir %s\n", taskpath);
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
           "%ld %ld %ld %llu",
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

    strcpy(data->comm, proc_info.comm);

    time_t run_time =
        time(0) - btime - (now[pid]->starttime / sysconf(_SC_CLK_TCK));

    if (prev[pid] && !strcmp(prev[pid]->comm, now[pid]->comm)) {
        long udelta = now[pid]->utime - prev[pid]->utime;
        long sdelta = now[pid]->stime - prev[pid]->stime;
        long base = env.delay * sysconf(_SC_CLK_TCK);

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

    fprintf(dest, "[PER-CPU]\n");
    fprintf(dest, "%7s %6s %6s %6s %10s\n", "cpu", "usr", "sys", "iowait",
            "delay(ns)");
    for (i = 1; i <= nr_cpu; i++) {
        char cpu_name[10];
        snprintf(cpu_name, 10, "cpu-%d", i - 1);
        fprintf(dest, "%7s %6.1f %6.1f %6.1f %10llu\n", cpu_name,
                sys->cpu[i].usr, sys->cpu[i].sys, sys->cpu[i].iowait,
                sys->percpu_sched_delay[i - 1]);
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
        fprintf(dest, "%20s %15d %15d %15llu %15d %15llu\n",
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

static void output_d_stack(struct record_t* rec, int d_num, FILE* dest) {
    int i;
    struct D_task_record_t* d_tasks = rec->d_tasks;
    for (i = 0; i < d_num; i++) {
        if (i == 0) {
            fprintf(dest, "[D-STASK]\n");
            fprintf(dest, "%18s %6s %6s %6s\n", "COMMAND", "PID", "PPID",
                    "STACK");
        }
        fprintf(dest, "%18s %6d %6d ", d_tasks[i].comm, d_tasks[i].tid,
                d_tasks[i].pid);

        char* str = d_tasks[i].stack;
        const char delim[2] = "\n";
        char* token;

        token = strtok(str, delim);
        fprintf(dest, "%s\n", token);

        while (true) {
            token = strtok(NULL, delim);
            if (!token) break;
            fprintf(dest, "%18s %6s %6s %s\n", "", "", "", token);
        }
    }
}

static bool check_cpu_overload(cpu_util_t* cpu) {
#define THRESHOLD_CPU_OVERLOAD 85
    double load = cpu->usr + cpu->sys + cpu->iowait;
    return load >= THRESHOLD_CPU_OVERLOAD;
}

static bool check_race(unsigned long long delay) {
#define THRESHOLD_DELAY 1 * 1000 * 1000 * 1000
    return delay > THRESHOLD_DELAY;
}

// static bool is_high_R(struct record_t* rec) {
// #define THRESHOLD_R nr_cpu * 1.5
//     return rec->sys.nr_R >= THRESHOLD_R;
// }

static bool is_high_D(struct record_t* rec) {
#define THRESHOLD_D 8
    return rec->sys.nr_D >= THRESHOLD_D;
}

void throttled_bind_detect(struct record_t* rec, FILE* dest) {
    cpu_util_t* cpus = rec->sys.cpu;
    unsigned long long* percpu_delay = rec->sys.percpu_sched_delay;
    bool status[nr_cpu];
    int i;
    for (i = 1; i <= nr_cpu; i++) {
        bool race = check_race(percpu_delay[i - 1]);
        status[i - 1] = race;

        bool overload = check_cpu_overload(cpus + i);
        if (race && overload)
            fprintf(dest, "CPU-%d is overload.\n", i - 1);
        else if (race && !overload)
            fprintf(dest, "CPU-%d maybe throttled. Please check cgroup.\n",
                    i - 1);
        else if (!race) {
#ifdef LOG_DEBUG
            fprintf(dest, "CPU-%d is normal.\n", i - 1);
#endif
        }
    }

    bool some_cores_race = false;  // some core is race?
    bool all_cores_race = true;    // all core is race?

    for (i = 0; i < nr_cpu; i++) {
        some_cores_race = some_cores_race || status[i];
        all_cores_race = all_cores_race && status[i];
    }

    if (all_cores_race) {
        /* all cores race, there is no bind */
        fprintf(dest, "System cpu resource is not enough.\n");
    } else if (some_cores_race) {
        fprintf(dest, "There maybe some tasks bind on cpu. Please check cpu:");
        for (i = 0; i < nr_cpu; i++) {
            if (status[i]) fprintf(dest, " [%d]", i);
        }
        fprintf(dest, "\n");
    } else {
        fprintf(dest, "System cpu is normal.\n ");
    }
}

static void fork_detect(struct record_t* rec, FILE* dest) {
#define THRESHOLD_FORK 4000
    if (rec->sys.nr_fork >= THRESHOLD_FORK) {
        fprintf(dest,
                "There are many forks, mayebe influence load1. Please check "
                "the task:comm=%s pid=%d ppid=%d\n",
                rec->sys.most_fork_info.comm, rec->sys.most_fork_info.pid,
                rec->sys.most_fork_info.ppid);
    }
}

static void D_detect(struct record_t* rec, FILE* dest) {
    /* IO or kernel resource？ */
    if (is_high_D(rec)) {
        fprintf(dest, "There are many D tasks, please analyse the D-stack.\n");
    }
}

static void decision(struct record_t* rec, int rec_num, FILE* dest, int d_num,
                     int cgroup_num) {
    fprintf(dest, "[EXCEPTION&ADVICE]\n");
    /* check throttled or bind */
    throttled_bind_detect(rec, dest);

    /* check fork */
    fork_detect(rec, dest);

    /* check D */
    D_detect(rec, dest);
}

static void output(struct record_t* rec, int rec_num, FILE* dest, int d_num,
                   int cgroup_num) {
    output_ts(dest);
    output_sys_load(rec, dest);
    output_per_cpu(rec, dest);
    output_cgroup(rec, cgroup_num, dest);
    output_tasktop(rec, rec_num, dest);
    output_d_stack(rec, d_num, dest);

    decision(rec, rec_num, dest, d_num, cgroup_num);
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
    u_int64_t i;
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

        /* many pair with the same pid, in process mode skip the trival read */
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
    u_int64_t total = 0;
    u_int64_t lookup_key = -1, next_key;
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

int main(int argc, char** argv) {
    int err = 0, fork_map_fd = -1;
    FILE* stat_log = 0;
    struct tasktop_bpf* skel = 0;
    struct id_pair_t* pids = 0;
    struct task_cputime_t **prev_task = 0, **now_task = 0;
    struct cgroup_cpu_stat_t** prev_cgroup = 0;
    struct sys_cputime_t **prev_sys = 0, **now_sys = 0;
    struct record_t* rec = 0;
    u_int64_t i;

    nr_cpu = sysconf(_SC_NPROCESSORS_ONLN);

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

    rec = calloc(1, sizeof(struct record_t));
    rec->sys.cpu = calloc(nr_cpu + 1, sizeof(struct cpu_util_t));
    rec->sys.percpu_sched_delay = calloc(nr_cpu, sizeof(int));
    rec->d_tasks = calloc(env.stack_limit, sizeof(struct D_task_record_t));
    rec->cgroups = calloc(env.cgroup_limit, sizeof(cgroup_cpu_stat_t));

    if (!rec || !rec->sys.cpu || !rec->sys.percpu_sched_delay ||
        !rec->d_tasks || !rec->cgroups) {
        err = 1;
        fprintf(stderr, "Failed calloc memory\n");
        goto cleanup;
    }

    prev_cgroup = calloc(env.cgroup_limit, sizeof(struct cgroup_cpu_stat_t*));

    prev_delay = calloc(nr_cpu, sizeof(int));
    pids = calloc(pidmax + 1, sizeof(struct id_pair_t));
    prev_task = calloc(pidmax + 1, sizeof(struct task_cputime_t*));
    now_task = calloc(pidmax + 1, sizeof(struct task_cputime_t*));
    prev_sys = calloc(1 + nr_cpu, sizeof(struct sys_cputime_t*));
    now_sys = calloc(1 + nr_cpu, sizeof(struct sys_cputime_t*));
    for (i = 0; i <= nr_cpu; i++) {
        prev_sys[i] = calloc(1, sizeof(struct sys_cputime_t));
        now_sys[i] = calloc(1, sizeof(struct sys_cputime_t));
    }

    if (!prev_task || !now_task || !prev_delay || !pids || !prev_sys ||
        !now_sys) {
        err = 1;
        fprintf(stderr, "Failed calloc memory.\n");
        goto cleanup;
    }

    prepare_directory(log_dir);
    stat_log = open_logfile();
    if (!stat_log) {
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

    err = tasktop_bpf__attach(skel);
    if (err) {
        fprintf(stderr, "Failed to attach BPF skeleton.\n");
        goto cleanup;
    }

    bool first = true;
    while (env.nr_iter-- && !exiting) {
        // printf("prev_sys=0x%x now_sys=0x%x\n", prev_sys, now_sys);
        u_int64_t nr_thread = 0;
        int rec_num = 0;
        int d_num = 0;
        int cgroup_num = 0;

#ifndef ONLY_THREAD
        read_cgroup_throttle(rec->cgroups, &cgroup_num, prev_cgroup);
        read_sched_delay(&rec->sys);
        check_fork(fork_map_fd, &rec->sys);
        runnable_proc(&rec->sys);
        unint_proc(&rec->sys);
        read_stat(prev_sys, now_sys, &rec->sys);
#endif

        /* get all process now */
        read_all_pids(pids, &nr_thread);

        read_d_task(pids, nr_thread, &d_num, rec->d_tasks);

#ifndef ONLY_THREAD
        rec->r_tasks = calloc(nr_thread, sizeof(struct R_task_record_t*));

        /* if prev process info exist produce record*/
        err = make_records(pids, nr_thread, rec, prev_task, now_task, &rec_num);
        if (err) {
            fprintf(stderr, "Failed make records.\n");
            goto cleanup;
        }

        /* sort record by sort type */
        sort_records(rec, rec_num, env.rec_sort);

        /* output record */
        if (!first)
            output(rec, rec_num, stat_log, d_num, cgroup_num);
        else
            first = false;

        free_records(rec, nr_thread);

        /* update old info and free nonexist process info */
        now_to_prev(pids, nr_thread, pidmax, prev_task, now_task, prev_sys,
                    now_sys);
#ifdef STRESS_TEST
        usleep(10000);
#else
        if (env.nr_iter) sleep(env.delay);
#endif
#endif
    }

cleanup:

    if (pids) free(pids);

    if (prev_task) {
        for (i = 0; i < pidmax; i++) {
            if (prev_task[i]) free(prev_task[i]);
        }
        free(prev_task);
    }

    if (now_task) {
        for (i = 0; i < pidmax; i++) {
            if (now_task[i]) free(now_task[i]);
        }
        free(now_task);
    }

    if (stat_log) fclose(stat_log);

    tasktop_bpf__destroy(skel);
    return err;
}
