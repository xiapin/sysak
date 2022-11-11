#ifndef OUTPUT_WARN_H
#define OUTPUT_WARN_H
#define PERCPU_TRIGER	1

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

int init_top_struct(struct top_utils *top, char *prefix); 
int record_top_util_proces(struct top_utils *top);
#endif
