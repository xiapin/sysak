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
#include <stdbool.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h> /* bpf_obj_pin */
#include "bpf/tasktop.skel.h"
#include "procstate.h"
#include "tasktop.h"
#include "common.h"

char log_dir[FILE_PATH_LEN] = "/var/log/sysak/tasktop";
char default_log_path[FILE_PATH_LEN] = "/var/log/sysak/tasktop/tasktop.log";
time_t btime = 0;
u_int64_t pidmax = 0;
char* log_path = 0;

struct env {
    bool thread_mode;
    time_t delay;
    pid_t tid;
    long nr_iter;
    enum sort_type rec_sort;
    int limit;
} env = {.thread_mode = false,
         .delay = 3,
         .tid = -1,
         .rec_sort = SORT_CPU,
         .nr_iter = LONG_MAX - 1,
         .limit = INT_MAX};

const char* argp_program_version = "tasktop 0.1";
const char argp_program_doc[] =
    "A light top, display the process/thread cpu utilization in peroid.\n"
    "\n"

    "USAGE: tasktop [--help] [-t] [-p TID] [-d DELAY] [-i ITERATION] [-s SORT] [-f LOGFILE] [-l "
    "LIMIT]\n"
    "\n"

    "EXAMPLES:\n"
    "    tasktop            # run forever, display the cpu utilization.\n"
    "    tasktop -t         # display all thread.\n"
    "    tasktop -p 1100    # only display task with pid 1100.\n"
    "    tasktop -d 5       # modify the sample interval.\n"
    "    tasktop -i 3       # output 3 times then exit.\n"
    "    tasktop -l 20      # limit the records number no more than 20.\n"
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
    {"limit", 'l', "LIMIT", 0, "Specify the top-LIMIT tasks to display"},
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

static int read_stat(struct sys_cputime_t* prev_sys, struct sys_cputime_t* now_sys,
                     struct sys_record_t* sys_rec) {
    int err = 0;
    FILE* fp = fopen(PROC_STAT_PATH, "r");
    if (!fp) {
        fprintf(stderr, "Failed open stat file.\n");
        err = errno;
        goto cleanup;
    }

    /*now only read first line, maybe future will read more info*/
    fscanf(fp, "%s %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld\n", now_sys->cpu, &now_sys->usr,
           &now_sys->nice, &now_sys->sys, &now_sys->idle, &now_sys->iowait, &now_sys->irq,
           &now_sys->softirq, &now_sys->steal, &now_sys->guest, &now_sys->guest_nice);

    if (prev_sys->usr == 0) goto cleanup;
    int now_time = now_sys->usr + now_sys->sys + now_sys->nice + now_sys->idle + now_sys->iowait +
                   now_sys->irq + now_sys->softirq + now_sys->steal + now_sys->guest +
                   now_sys->guest_nice;
    int prev_time = prev_sys->usr + prev_sys->sys + prev_sys->nice + prev_sys->idle +
                    prev_sys->iowait + prev_sys->irq + prev_sys->softirq + prev_sys->steal +
                    prev_sys->guest + prev_sys->guest_nice;
    int all_time = now_time - prev_time;
    // int all_time = (sysconf(_SC_NPROCESSORS_ONLN) * env.delay * sysconf(_SC_CLK_TCK));

    /* all_time can't not calculate by delay * ticks * online-cpu-num, because there is error
     * between process waked up and running, when sched delay occur , the sum of cpu rates more than
     * 100%. */

    sys_rec->usr = (double)(now_sys->usr - prev_sys->usr) * 100 / all_time;
    sys_rec->sys = (double)(now_sys->sys - prev_sys->sys) * 100 / all_time;
    sys_rec->iowait = (double)(now_sys->iowait - prev_sys->iowait) * 100 / all_time;

cleanup:
    if (fp) fclose(fp);
    return err;
};

static u_int64_t read_pid_max() {
    int err = 0;
    FILE* fp = fopen(PIDMAX_PATH, "r");
    if (!fp) {
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
    u_int64_t proc_num = 0;
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
            snprintf(taskpath, FILE_PATH_LEN, "/proc/%d/task", pid);
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

static int read_proc(pid_t pid, pid_t tid, struct task_cputime_t** prev,
                     struct task_cputime_t** now, struct task_record_t** rec) {
    struct proc_stat_t proc_info;
    char proc_path[FILE_PATH_LEN];
    struct task_cputime_t* data;
    FILE* fp = 0;
    int err = 0;

    if (tid != -1) {
        snprintf(proc_path, FILE_PATH_LEN, "/proc/%d/task/%d/stat", pid, tid);
        pid = tid;
    } else {
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
        long base = env.delay * sysconf(_SC_CLK_TCK);

        if (base != 0) {
            /* only process cpu utilization > 0 */
            if (udelta + sdelta > 0) {
                *rec = calloc(1, sizeof(struct task_record_t));
                (*rec)->pid = now[pid]->pid;
                (*rec)->ppid = now[pid]->ppid;
                (*rec)->runtime = run_time;
                (*rec)->begin_ts = btime + now[pid]->starttime / sysconf(_SC_CLK_TCK);
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

static void sort_records(struct record_t* rec, int proc_num, enum sort_type sort) {
    struct task_record_t** records = rec->tasks;
    int i, j;
    for (i = 0; i < proc_num; i++) {
        for (j = i + 1; j < proc_num; j++) {
            if (!records[j] && !records[i]) {
                continue;
            } else if (records[i] && !records[j]) {
                continue;
            } else if (!records[i] && records[j]) {
                swap(&records[i], &records[j], sizeof(struct task_record_t*));
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
                    swap(&records[i], &records[j], sizeof(struct task_record_t*));
                }
            }
        }
    }
}

static char* ts2str(time_t ts, char* buf, int size) {
    // __builtin_memset(buf, 0, size;
    struct tm* t = gmtime(&ts);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", t);
    return buf;
}

static char* second2str(time_t ts, char* buf, int size) {
    // printf("%ld\n", ts);
#define MINUTE 60
#define HOUR (MINUTE * 60)
#define DAY (HOUR * 24)
    // int seconds = ts;
    memset(buf, 0, size);
    int i = 0;
    int day = (int)ts / DAY;
    ts = ts % DAY;
    int hour = (int)ts / HOUR;
    ts = ts % HOUR;
    int minute = (int)ts / MINUTE;
    ts = ts % MINUTE;

    snprintf(buf, size, "%dd:%dh:%dm:%lds", day, hour, minute, ts);
    for (i = 0; i < size; i++) {
        if (buf[i] != ':') break;

        if (i == 0)
            buf[i] = ' ';
        else if (buf[i - 1] == ' ')
            buf[i] = ' ';
    }

    // printf("%s\n", buf);
    return buf;
}

static void output(struct record_t* rec, int proc_num, FILE* dest) {
    struct task_record_t** records = rec->tasks;
    // system("clear");
    time_t now = time(0);

    char stime_str[BUF_SIZE] = {0};
    char rtime_str[BUF_SIZE] = {0};
    int i;
    struct proc_fork_info_t* info = &(rec->sys.most_fork_info);

    fprintf(dest, "%s\n", ts2str(now, stime_str, BUF_SIZE));
    fprintf(dest, "UTIL&LOAD\n");
    fprintf(dest, "%6s %6s %6s %6s %6s %6s %6s :%5s \n", "usr", "sys", "iowait", "load1", "R", "D",
            "fork", "proc");

    fprintf(dest, "%6.1f %6.1f %6.1f %6.1f %6d %6d %6d", rec->sys.usr, rec->sys.sys,
            rec->sys.iowait, rec->sys.load1, rec->sys.nr_R, rec->sys.nr_D, rec->sys.nr_fork);
    fprintf(dest, " : %s(%d) ppid=%d cnt=%lu \n", info->comm, info->pid, info->ppid, info->fork);

    for (i = 0; i < proc_num; i++) {
        if (!records[i]) break;

        if (i == 0) {
            fprintf(dest, "TASKTOP\n");
            fprintf(dest, "%18s %6s %6s %20s %10s %6s %6s %6s\n", "COMMAND", "PID", "PPID", "START",
                    "RUN", "%UTIME", "%STIME", "%CPU");
        }

        if (i >= env.limit) break;
        fprintf(dest, "%18s %6d %6d %20s %10ld %6.1f %6.1f %6.1f\n", records[i]->comm,
                records[i]->pid, records[i]->ppid,
                ts2str(records[i]->begin_ts, stime_str, BUF_SIZE), records[i]->runtime,
                records[i]->user_cpu_rate, records[i]->system_cpu_rate, records[i]->all_cpu_rate);
    }
    fflush(dest);
}

static void now_to_prev(struct id_pair_t* pids, int proc_num, int pidmax,
                        struct task_cputime_t** prev_task, struct task_cputime_t** now_task,
                        struct sys_cputime_t* prev_sys, struct sys_cputime_t* now_sys) {
    int i;
    for (i = 0; i < pidmax; i++) {
        if (prev_task[i]) {
            free(prev_task[i]);
            prev_task[i] = NULL;
        }
    }

    for (i = 0; i < proc_num; i++) {
        int pid;
        if (env.thread_mode)
            pid = pids[i].tid;
        else
            pid = pids[i].pid;
        swap(&prev_task[pid], &now_task[pid], sizeof(struct task_cputime_t*));
    }

    swap(prev_sys, now_sys, sizeof(struct sys_cputime_t));
}

static int make_records(struct id_pair_t* pids, int proc_num, struct record_t* rec,
                        struct task_cputime_t** prev_task, struct task_cputime_t** now_task,
                        struct sys_cputime_t* prev_sys, struct sys_cputime_t* now_sys) {
    struct task_record_t** records = rec->tasks;
    int err = 0;
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

        err = read_proc(id->pid, id->tid, prev_task, now_task, &records[i]);
        if (err) {
            fprintf(stderr, "Failed read proc\n");
            return err;
        }
    }
    read_stat(prev_sys, now_sys, &rec->sys);

    return err;
}

static void free_records(struct record_t* rec, int proc_num) {
    struct task_record_t** records = rec->tasks;
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

static int libbpf_print_fn(enum libbpf_print_level level, const char* format, va_list args) {
    return vfprintf(stderr, format, args);
}

static int bump_memlock_rlimit(void) {
    struct rlimit rlim_new = {
        .rlim_cur = RLIM_INFINITY,
        .rlim_max = RLIM_INFINITY,
    };

    return setrlimit(RLIMIT_MEMLOCK, &rlim_new);
}

static int look_fork(int cnt_map_fd, int fork_map_fd, struct sys_record_t* sys_rec) {
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
            fprintf(stderr, "failed to delete elem: %d\n", err);
            return -1;
        }
        lookup_key = next_key;

        if (!next_key) continue;

        total = total + info.fork;  // for debug

        if (max_fork < info.fork) {
            max_fork = info.fork;
            sys_rec->most_fork_info = info;
        }
    }
    sys_rec->nr_fork = total;
    return err;
}

int main(int argc, char** argv) {
    int err = 0;
    int cnt_map_fd = -1, fork_map_fd = -1;
    FILE* stat_log = 0;
    struct tasktop_bpf* skel = 0;
    struct id_pair_t* pids = 0;
    struct task_cputime_t **prev_task = 0, **now_task = 0;
    struct sys_cputime_t *prev_sys = 0, *now_sys = 0;
    struct record_t* rec = 0;

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
    pids = calloc(pidmax, sizeof(struct id_pair_t));
    prev_task = calloc(pidmax, sizeof(struct task_cputime_t*));
    now_task = calloc(pidmax, sizeof(struct task_cputime_t*));
    prev_sys = calloc(1, sizeof(struct sys_cputime_t));
    now_sys = calloc(1, sizeof(struct sys_cputime_t));
    if (!prev_task || !now_task) {
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

    skel = tasktop_bpf__open();
    if (!skel) {
        err = 1;
        fprintf(stderr, "Failed to open BPF skeleton\n");
        goto cleanup;
    }

    err = tasktop_bpf__load(skel);
    if (err) {
        fprintf(stderr, "Failed to load BPF skeleton\n");
        goto cleanup;
    }

    cnt_map_fd = bpf_map__fd(skel->maps.cnt_map);
    fork_map_fd = bpf_map__fd(skel->maps.fork_map);
    err = tasktop_bpf__attach(skel);
    if (err) {
        fprintf(stderr, "Failed to attach BPF skeleton\n");
        goto cleanup;
    }

    bool first = true;
    while (env.nr_iter--) {
        look_fork(cnt_map_fd, fork_map_fd, &rec->sys);
        runnable_proc(&rec->sys);
        unint_proc(&rec->sys);

        /* get all process now */
        u_int64_t proc_num;
        err = read_all_pids(pids, &proc_num);
        if (err) {
            fprintf(stderr, "Failed read all pids.\n");
            goto cleanup;
        }

        rec->tasks = calloc(proc_num, sizeof(struct task_record_t*));
        /* if prev process info exist produce record*/
        err = make_records(pids, proc_num, rec, prev_task, now_task, prev_sys, now_sys);
        if (err) {
            fprintf(stderr, "Failed make records.\n");
            goto cleanup;
        }

        /* sort record by sort type */
        sort_records(rec, proc_num, env.rec_sort);

        /* output record */
        if (!first)
            output(rec, proc_num, stat_log);
        else
            first = false;

        free_records(rec, proc_num);

        /* update old info and free nonexist process info */
        now_to_prev(pids, proc_num, pidmax, prev_task, now_task, prev_sys, now_sys);

        if (env.nr_iter) sleep(env.delay);
    }

cleanup:
    tasktop_bpf__destroy(skel);

    if (pids) free(pids);
    u_int64_t i;
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

    return err;
}
