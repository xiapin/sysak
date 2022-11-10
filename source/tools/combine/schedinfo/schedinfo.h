#define BPF_ANY	0
#define MAX_MONI_NR	1024

/* latency thresh:10ms*/
#define LAT_THRESH_NS	(10*1000*1000)
#define TASK_COMM_LEN	16
#define PERF_MAX_STACK_DEPTH	32
#define STAT_PATH	"/proc/stat"
#define SYSAK_LOG_PATH	"/usr/local/sysak/log/"

struct args {
	int flag;
	__u64 thresh;
};

struct ksym {
	long addr;
	char *name;
};

struct key_t {
	__u32 ret;
};

struct rq_info {
	__u32 cpu;
	unsigned int nr_running;
	unsigned long  nr_uninterruptible;
	unsigned long long rq_cpu_time;
	unsigned long pcount;
	unsigned long long run_delay;
};

struct sched_datas {
	char cpuid[8];
	__u64 delay, pcnt, nr_running, total;
	__u64 user, nice, sys, idle, iowait, hirq, sirq, steal, guest;
	double idle_util, user_util, nice_util, sys_util, iowait_util, hirq_util, sirq_util;
};

struct ps_info {
	long pid, tid;
	float cpu;
	char etime[14], time[10];
	char state[4], comm[16];
};

struct top_utils {
	int index, nr;
	struct ps_info *pss;
	void *log;
	struct tm last_dt;
};

struct schedinfo {
	__u32 warnbits;
	unsigned long nr_uninterruptible;
	__u64 nr_forked, nr_block;
	struct top_utils top;
	struct sched_datas *datass, *prev, *allcpu, *allcpuprev;
};

struct latinfo {
	__u64 last_seen_need_resched_ns;
	__u64 last_perf_event;
	__u64 thresh;
	int ticks_without_resched;
};

