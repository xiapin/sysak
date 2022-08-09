#ifndef __RECLAIMHUNG_H
#define __RECLAIMHUNG_H

#define HOOK_FAILED 1
#define PROC_NUMBUF 13
#define TASK_NUM    20
#define TASK_COMM_LEN   16
#define DEF_TIME    10 /* s */
#define MS_TO_NS    (1000*1000)
#define SEC_TO_NS   (1000*1000*1000)

enum map_class {
	C_RRECLAIM,
	C_COMPACT,
	C_CGROUP_RECLAIM,
};
	
struct data_t{
    __u32 pid;
    __u32 tgid;
    __u64 ts_begin;  //start time
    __u64 ts_delay;    // end time
    char comm[TASK_COMM_LEN];
    __u64 time;
};

struct reclaim_data {
    int nr_reclaimed;
    int nr_pages;
	__u32 cgroup;
    struct data_t da;
};

struct compact_data {
    int nr_compacted;
    int status;
    int cgroup;
    struct data_t da;
};
#endif