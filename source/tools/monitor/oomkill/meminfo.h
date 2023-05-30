/* SPDX-License-Identifier: MIT */
#ifndef MEMINFO_H
#define MEMINFO_H
#include <stdbool.h>
#include <sys/types.h>

#define PATH_LEN 256

#define NOR_KSIZE (8*1024*1024) //8G

#define WARN_KSIZE (6.4*1024*1024) //6.4G
#define KILL_KSIZE (5*1024*1024) //5G

#define WARN_RATE (0.10) //10%
#define NOR_RATE (0.12) //12%


#define KILL_CACHE_KSIZE (3*1024*1024)
#define KILL_CACHE_RATE  (0.05) //%5

/*
 * NORMAL: MemAvailable > memToal*10%
 * WARN:   MemAvailable < memTotal*10% 
 * CRI:    kswaped is active for memory reclaim
 * ALERT:  system entry direct memory reclaim
 * EMER :  system is block for direct memory reclaim(may oom kill)
 * */
typedef enum {NORMAL=1,WARN,CRIT,ALERT,EMER} memstatus;

typedef struct {
    // Values from /proc/meminfo, in KiB
    long long MemTotalKiB;
    long long MemFreeKiB;
    long long MemFileCacheKiB;
    long long MemAvailableKiB;
    long long SwapTotalKiB;
    long long SwapFreeKiB;
    long long AnonPagesKiB;
    // Calculated values
    // UserMemTotalKiB = MemAvailableKiB + AnonPagesKiB.
    // Represents the total amount of memory that may be used by user processes.
    long long UserMemTotalKiB;
    // Calculated percentages
    double MemAvailablePercent; // percent of total memory that is available
    double SwapFreePercent; // percent of total swap that is free
} meminfo_t;

typedef struct procinfo {
    int pid;
    int uid;
    long badness;
    int oom_score_adj;
    long long VmRSSkiB;
    char name[PATH_LEN];
} procinfo_t;

meminfo_t parse_meminfo();
bool is_alive(int pid);
void print_mem_stats(int (*out_func)(const char* fmt, ...), const meminfo_t m);
int get_oom_score(int pid);
int get_oom_score_adj(const int pid, int* out);
long long get_vm_rss_kib(int pid);
int get_comm(int pid, char* out, size_t outlen);
int get_uid(int pid);

#endif
