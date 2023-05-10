#define BPF_ANY	0
#define MAX_MONI_NR	1024
/* latency thresh:10ms*/
#define LAT_THRESH_NS	(10*1000*1000)
#define TASK_COMM_LEN	16

struct args {
	int flag;
	__u64 thresh;
};

struct key_t {
	__u32 ret;
};

struct latinfo {
	__u64 last_seen_need_resched_ns;
	__u64 last_perf_event;
	int ticks_without_resched;
};

struct wake_up_data {
	union {
		pid_t wakee, waker;
	};
	pid_t ppid;
	union {
		__u64 new_cnt, wake_cnt;
	};
	char comm[16];
};

struct fds {
	int cntfd, forkfd, wakefd;
};

struct wake_account {
	__u64 new_cnt, wake_cnt;
};
