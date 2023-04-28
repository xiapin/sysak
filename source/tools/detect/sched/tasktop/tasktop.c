#include <argp.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#define FILE_PATH_LEN 256
#define MAX_COMM_LEN 16
#define PEROID 3
#define LIMIT 20
#define BUF_SIZE 512

// #define DEBUG 1
#define DEBUG_LOG "./log/debug.log"
char log_dir[FILE_PATH_LEN] = "/var/log/sysak/tasktop";
char default_log_path[FILE_PATH_LEN] = "/var/log/sysak/tasktop/tasktop.log";

#define PIDMAX_PATH "/proc/sys/kernel/pid_max"
#define PROC_STAT_PATH "/proc/stat"

time_t btime = 0;
u_int64_t pidmax = 0;
char* log_path = 0;

enum sort_type { SORT_SYSTEM, SORT_USER, SORT_CPU };

struct id_pair_t {
    pid_t pid;
    pid_t tid;
};

struct proc_stat_t {
    int pid;
    char comm[16];
    char state;
    int ppid;
    int pgrp;
    int session;
    int tty_nr;
    int tpgid;
    unsigned int flags;
    u_int64_t minflt;
    u_int64_t cminflt;
    u_int64_t majflt;
    u_int64_t cmajflt;
    u_int64_t utime;
    u_int64_t stime;
    int64_t cutime;
    int64_t cstime;
    int64_t priority;
    int64_t nice;
    int64_t num_threads;
    int64_t itrealvalue;
    unsigned long long starttime;
};

struct cpu_time_t {
    int pid;
    int ppid;
    char comm[MAX_COMM_LEN];
    u_int64_t stime;
    u_int64_t utime;
    u_int64_t starttime;
};

struct record_t {
    u_int64_t pid;
    u_int64_t ppid;
    char comm[MAX_COMM_LEN];
    time_t runtime;
    double system_cpu_rate;
    double user_cpu_rate;
    double all_cpu_rate;
};

struct env {
    bool thread_mode;
    time_t delay;
    pid_t tid;
    long nr_iter;
    enum sort_type rec_sort;
} env = {
    .thread_mode = false, .delay = 3, .tid = -1, .rec_sort = SORT_CPU, .nr_iter = LONG_MAX - 1};

const char* argp_program_version = "tasktop 0.1";
const char argp_program_doc[] =
    "A light top, display the process/thread cpu utilization in peroid.\n"
    "\n"

    "USAGE: tasktop [--help] [-t] [-p TID] [-d DELAY] [-i ITERATION] [-s SORT] [-f LOGFILE]\n"
    "\n"

    "EXAMPLES:\n"
    "    tasktop            # run forever, display the cpu utilization.\n"
    "    tasktop -t         # display all thread.\n"
    "    tasktop -p 1100    # only display task with pid 1100.\n"
    "    tasktop -d 5       # modify the sample interval.\n"
    "    tasktop -i 3       # output 3 times then exit.\n"
    "    tasktop -f a.log   # log to a.log (default to /var/log/sysak/tasktop/tasktop.log)\n";

static const struct argp_option opts[] = {
    {"thread", 't', 0, 0, "Thread mode, default process"},
    {"pid", 'p', "TID", 0, "Specify thread TID"},
    {"delay", 'd', "DELAY", 0, "Sample peroid, default is 3 seconds"},
    {"iter", 'i', "ITERATION", 0, "Output times, default run forever"},
    {"logfile", 'f', "LOGFILE", 0,
     "Logfile for result, default /var/log/sysak/tasktop/tasktop.log"},
    {"sort", 's', "SORT", 0,
     "Sort the result, available options are user, sys and cpu, default is cpu"},

    {NULL, 'h', NULL, OPTION_HIDDEN, "Show the full help"},
    {},
};

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
    if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) || (errno != 0 && val == 0)) {
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
            if (err) {
                fprintf(stderr, "Failed parse delay.\n");
                argp_usage(state);
            }
            env.delay = val;
            break;
        case 'i':
            err = parse_long(arg, &val);
            if (err) {
                fprintf(stderr, "Failed parse pid.\n");
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
    char* endptr;
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

static void swap_cpu_time(struct cpu_time_t** lth, struct cpu_time_t** rth) {
    struct cpu_time_t* tmp = *lth;
    *lth = *rth;
    *rth = tmp;
}

static void swap_record(struct record_t** lth, struct record_t** rth) {
    struct record_t* tmp = *lth;
    *lth = *rth;
    *rth = tmp;
}

static u_int64_t read_pid_max() {
    int err = 0;
    FILE* fp = fopen(PIDMAX_PATH, "r");
    if (!fp) {
        err = errno;
        return err;
    }

    fscanf(fp, "%ul", &pidmax);

    if (fp) fclose(fp);
    return err;
}

static int read_all_pids(struct id_pair_t* pids, u_int64_t* num) {
    int err = 0;

    DIR* dir = NULL;
    DIR* task_dir = NULL;
    u_int64_t proc_num = 0;
    char* endptr;
    struct dirent* proc_de = NULL;
    struct dirent* task_de = NULL;
    long val;
    pid_t pid, tid;

    dir = opendir("/proc");
    if (!dir) {
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
        if (!env.thread_mode) {
            pids[proc_num].pid = pid;
            pids[proc_num++].tid = -1;
        } else {
            char taskpath[FILE_PATH_LEN];
            snprintf(taskpath, FILE_PATH_LEN, "/proc/%ld/task", pid);
            task_dir = opendir(taskpath);
            if (!task_dir) {
                if (errno == ENOENT) {
                    continue;
                }
                err = errno;
                goto cleanup;
            }

            while ((task_de = readdir(task_dir)) != NULL) {
                if (task_de->d_type != DT_DIR || !strcmp(task_de->d_name, ".") ||
                    !strcmp(task_de->d_name, ".."))
                    continue;
                err = parse_long(task_de->d_name, &val);

                if (err) {
                    fprintf(stderr, "Failed parse tid\n");
                    goto cleanup;
                }
                tid = val;

                pids[proc_num].pid = pid;
                pids[proc_num++].tid = tid;
            }

            if (task_dir) {
                closedir(task_dir);
                task_dir = NULL;
            }
        }
    }
    *num = proc_num;
cleanup:
    if (dir) closedir(dir);
    if (task_dir) closedir(task_dir);
    return err;
}

static int read_proc(pid_t pid, pid_t tid, struct cpu_time_t** prev, struct cpu_time_t** now,
                     struct record_t** rec) {
    struct proc_stat_t proc_info;
    char proc_path[FILE_PATH_LEN];
    struct cpu_time_t* data;
    FILE* fp = 0;
    int err = 0;
#ifdef DEBUG
    // printf("read proc pid =%d tid=%d\n", pid, tid);
#endif
    if (tid != -1) {
        snprintf(proc_path, FILE_PATH_LEN, "/proc/%d/task/%d/stat", pid, tid);
        pid = tid;
    } else {
        snprintf(proc_path, FILE_PATH_LEN, "/proc/%d/stat", pid);
    }

    if (!now[pid]) {
        now[pid] = calloc(1, sizeof(struct cpu_time_t));
        if (!now[pid]) {
            fprintf(stderr, "Failed calloc memory.\n");
            err = errno;
            goto cleanup;
        }
    }
    data = now[pid];

    fp = fopen(proc_path, "r");
    if (!fp) {
#ifdef DEBUG
        // fprintf(stderr, "Failed open proc stat. Process-[%s] is exit.\n", now[pid]->comm);
#endif
        // err = errno;
        if (prev[pid]) free(prev[pid]);
        if (now[pid]) free(now[pid]);

        prev[pid] = NULL;
        now[pid] = NULL;
        goto cleanup;
    }

    fscanf(fp,
           "%d %s %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu %ld %ld %ld "
           "%ld %ld %ld %llu",
           &proc_info.pid, &proc_info.comm[0], &proc_info.state, &proc_info.ppid, &proc_info.pgrp,
           &proc_info.session, &proc_info.tty_nr, &proc_info.tpgid, &proc_info.flags,
           &proc_info.minflt, &proc_info.cminflt, &proc_info.majflt, &proc_info.cmajflt,
           &proc_info.utime, &proc_info.stime, &proc_info.cutime, &proc_info.cstime,
           &proc_info.priority, &proc_info.nice, &proc_info.num_threads, &proc_info.itrealvalue,
           &proc_info.starttime);

    data->utime = proc_info.utime;
    data->stime = proc_info.stime;
    data->ppid = proc_info.ppid;
    data->starttime = proc_info.starttime;
    data->pid = proc_info.pid;
    strcpy(data->comm, proc_info.comm);

    time_t run_time = time(0) - btime - (now[pid]->starttime / sysconf(_SC_CLK_TCK));

    if (prev[pid] && !strcmp(prev[pid]->comm, now[pid]->comm)) {
        long udelta = now[pid]->utime - prev[pid]->utime;
        long sdelta = now[pid]->stime - prev[pid]->stime;
        long base = PEROID * sysconf(_SC_CLK_TCK);

        if (base != 0) {
            /* only process cpu utilization > 0 */
            if (udelta + sdelta > 0) {
                *rec = calloc(1, sizeof(struct record_t));
                (*rec)->pid = now[pid]->pid;
                (*rec)->ppid = now[pid]->ppid;
                (*rec)->runtime = run_time;
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

static void sort_records(struct record_t** records, int proc_num, enum sort_type sort) {
#ifdef DEBUG
    // printf("sort_records procnum=%d\n", proc_num);
#endif
    int i, j;
    for (i = 0; i < proc_num; i++) {
        for (j = i + 1; j < proc_num; j++) {
            if (!records[j] && !records[i]) {
                continue;
            } else if (records[i] && !records[j]) {
                continue;
            } else if (!records[i] && records[j]) {
                swap_record(&records[i], &records[j]);
#ifdef DEBUG
                // printf("swap record i=%d j=%d\n", i, j);
#endif
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
                        break;
                }

                if (lth < rth) {
                    swap_record(&records[i], &records[j]);
#ifdef DEBUG
                    // printf("swap record i=%d j=%d\n", i, j);
#endif
                }
            }
        }
    }
}

static void output(struct record_t** records, int proc_num, FILE* dest) {
#ifndef DEBUG
    system("clear");
#endif
    time_t now = time(0);
    struct tm* t;
    t = gmtime(&now);
    char time_str[BUF_SIZE];
    int i;
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);
    for (i = 0; i < proc_num; i++) {
        if (!records[i]) break;

        if (i == 0) {
            fprintf(dest, "%s\n", time_str);
            fprintf(dest, "%18s %6s %6s %10s %6s %6s %6s\n", "COMMAND", "PID", "PPID", "RUNTIME",
                    "%UTIME", "%STIME", "%CPU");
        }
        
        fprintf(dest, "%18s %6d %6d %10d %6.2f %6.2f %6.2f\n", records[i]->comm, records[i]->pid,
                records[i]->ppid, records[i]->runtime, records[i]->user_cpu_rate,
                records[i]->system_cpu_rate, records[i]->all_cpu_rate);
    }
    fflush(dest);
}

static void clean_prev_table(int pidmax, struct cpu_time_t** prev, struct cpu_time_t** now) {
    int i;
    for (i = 0; i < pidmax; i++) {
        if (prev[i]) free(prev[i]);
    }
}

static void now_to_prev(struct id_pair_t* pids, int proc_num, int pidmax, struct cpu_time_t** prev,
                        struct cpu_time_t** now) {
    int i;
    for (i = 0; i < pidmax; i++) {
        if (prev[i]) {
            free(prev[i]);
            prev[i] = NULL;
        }
    }

    for (i = 0; i < proc_num; i++) {
        int pid;
        if (env.thread_mode)
            pid = pids[i].tid;
        else
            pid = pids[i].pid;
        swap_cpu_time(&prev[pid], &now[pid]);
    }
}

static int make_records(struct id_pair_t* pids, int proc_num, struct record_t*** res,
                        struct cpu_time_t** prev, struct cpu_time_t** now) {
    struct record_t** records = *res;
    int err = 0;
    // todo 可以优化一下真正读到的record 降低排序耗时
    u_int64_t i;
    for (i = 0; i < proc_num; i++) {
        struct id_pair_t* id = &pids[i];

        if (env.tid != -1) {
            if (env.thread_mode) {
                if (id->tid != env.tid) continue;
            } else {
                if (id->pid != env.tid) continue;
            }
        }

        err = read_proc(id->pid, id->tid, prev, now, &records[i]);
        if (err) {
            fprintf(stderr, "Failed read proc\n");
            return err;
        }
    }

    return err;
}

static void free_records(struct record_t** records, int proc_num) {
    int i;
    for (i = 0; i < proc_num; i++) {
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

int main(int argc, char** argv) {
    int err = 0;
    FILE* stat_log;
    struct id_pair_t* pids;
    struct cpu_time_t **prev, **now;

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

    pids = calloc(pidmax, sizeof(struct id_pair_t));
    prev = calloc(pidmax, sizeof(struct cpu_time_t*));
    now = calloc(pidmax, sizeof(struct cpu_time_t*));
    if (!prev || !now) {
        err = errno;
        fprintf(stderr, "Failed calloc prev and now\n");
        goto cleanup;
    }

    prepare_directory(log_dir);

    stat_log = open_logfile();
    if (!stat_log) {
        fprintf(stderr, "Failed open stat log file.\n");
        goto cleanup;
    }

#ifdef DEBUG
    FILE* debug_fp = fopen(DEBUG_LOG, "w");
    if (!debug_fp) fprintf(stderr, "Failed open debug log file.\n");
#endif

    while (env.nr_iter--) {
#ifdef DEBUG
        struct timeval start, end;
        err = gettimeofday(&start, 0);
        if (err) fprintf(stderr, "Failed get time.\n");
#endif
        /* get all process now */
        u_int64_t proc_num;
        err = read_all_pids(pids, &proc_num);
        if (err) {
            fprintf(stderr, "Failed read all pids.\n");
            goto cleanup;
        }
        struct record_t** records = calloc(proc_num, sizeof(struct recotd_t*));
        /* if prev process info exist produce record*/
        err = make_records(pids, proc_num, &records, prev, now);
        if (err) {
            fprintf(stderr, "Failed make records.\n");
            goto cleanup;
        }

        /* sort record by sort type */
        sort_records(records, proc_num, env.rec_sort);

        /* output record */
        output(records, proc_num, stat_log);

        free_records(records, proc_num);

        /* update old info and free nonexist process info */
        now_to_prev(pids, proc_num, pidmax, prev, now);
#ifdef DEBUG
        err = gettimeofday(&end, 0);
        if (err) fprintf(stderr, "Failed get time.\n");
        suseconds_t interval = end.tv_usec - start.tv_usec;
        fprintf(debug_fp, "scan %d process, time=%d us(%.3f ms)\n", proc_num, interval,
                (double)interval / 1000);
        fflush(debug_fp);
#endif
        if (env.nr_iter) sleep(env.delay);
    }

cleanup:
    if (pids) free(pids);
    u_int64_t i;
    if (prev) {
        for (0; i < pidmax; i++) {
            if (prev[i]) free(prev[i]);
        }
        free(prev);
    }

    if (now) {
        for (i = 0; i < pidmax; i++) {
            if (now[i]) free(now[i]);
        }
        free(now);
    }

    if (stat_log) fclose(stat_log);

    return err;
}
