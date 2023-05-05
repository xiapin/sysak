// #define DEBUG 1
#define FILE_PATH_LEN 256
#define MAX_COMM_LEN 16
#define PEROID 3
#define LIMIT 20
#define CPU_NAME_LEN 8
#define BUF_SIZE 512
#define DEBUG_LOG "./log/debug.log"
#define PIDMAX_PATH "/proc/sys/kernel/pid_max"
#define PROC_STAT_PATH "/proc/stat"

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

struct task_cputime_t {
    int pid;
    int ppid;
    char comm[MAX_COMM_LEN];
    u_int64_t stime;
    u_int64_t utime;
    u_int64_t starttime;
};

struct sys_cputime_t {
    char cpu[CPU_NAME_LEN];
    long usr;
    long nice;
    long sys;
    long idle;
    long iowait;
    long irq;
    long softirq;
    long steal;
    long guest;
    long guest_nice;
};

struct task_record_t {
    u_int64_t pid;
    u_int64_t ppid;
    char comm[MAX_COMM_LEN];
    time_t runtime;
    double system_cpu_rate;
    double user_cpu_rate;
    double all_cpu_rate;
};

struct sys_record_t {
    double usr;
    double sys;
    double iowait;
    int nr_R;
    int nr_D;
    int nr_fork;
};

struct record_t {
    struct task_record_t **tasks;
    struct sys_record_t sys;
};