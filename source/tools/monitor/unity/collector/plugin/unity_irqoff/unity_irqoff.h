#ifndef __IRQOFF_H
#define __IRQOFF_H

#define TASK_COMM_LEN	16
#define CPU_ARRY_LEN	4
#define CONID_LEN	13

struct info {
	__u64 prev_counter;
};

struct tm_info {
	__u64 last_stamp;
};

struct arg_info {
	__u64 thresh;
};

#ifndef __VMLINUX_H__

#include "../plugin_head.h"

#define DEFINE_SEKL_OBJECT(skel_name)                            \
    struct skel_name##_bpf *skel_name = NULL;                    \
    static pthread_t perf_thread = 0;                            \
    int thread_worker(struct beeQ *q, void *arg)                 \
    {                                                            \
        perf_thread_worker(arg);                                 \
        return 0;                                                \
    }                                                            \
    void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt)  \
    {                                                            \
        printf("Lost %llu events on CPU #%d!\n", lost_cnt, cpu); \
    }

#define DESTORY_SKEL_BOJECT(skel_name) \
    if (perf_thread > 0)               \
        kill_perf_thread(perf_thread); \
    skel_name##_bpf__destroy(skel_name);

int init(void *arg);
int call(int t, struct unity_lines *lines);
void deinit(void);

#endif
#endif /* __IRQOFF_H */

