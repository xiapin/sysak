#ifndef __ACLTRACE_H
#define __ACLTRACE_H

#define FILE_NAME_LEN   64
#define TASK_COMM_LEN   16
#define DEF_TIME    10 /* s */
#define MS_TO_NS    (1000*1000)
#define SEC_TO_NS   (1000*1000*1000)

enum map_class {
	C_RRECLAIM,
	C_COMPACT,
	C_CGROUP_RECLAIM,
};
	
struct acl_data{
    __u32 pid;
    __u32 tgid;
    __u32 count;
    char dentryname[FILE_NAME_LEN];
    char xattrs[FILE_NAME_LEN];
    char comm[TASK_COMM_LEN];
    __u64 time;
};
#endif