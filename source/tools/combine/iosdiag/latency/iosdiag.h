#ifndef __IOSDIAG__
#define __IOSDIAG__

#define	COUNT			(10)
#define	BPF_ANY			(0)
#define	IOSDIAG_PKG_MAGIC	0x494F5049

#define REQ_OP_BITS		8
#define REQ_OP_MASK		((1 << REQ_OP_BITS) - 1)
#define MAX_STACK_DEPTH		12

enum ioroute_type{
	IO_START_POINT,
	IO_ISSUE_DRIVER_POINT,
	IO_ISSUE_DEVICE_POINT,
	IO_RESPONCE_DRIVER_POINT,
	IO_COMPLETE_TIME_POINT,
	IO_DONE_POINT,
	MAX_POINT,
};

enum operating_mode{
	DIAGNOSTIC_MODE,
	MONITOR_MODE,
};

struct iosdiag_req {
	pid_t pid;
	unsigned int queue_id;
	char comm[16];
	char diskname[32];
	unsigned long long ts[MAX_POINT];
	unsigned int cpu[4];
	//unsigned int complete;
	//unsigned int cmd_flags;
	char op[8];
	unsigned int data_len;
	unsigned long sector;
};

struct aggregation_metrics {
	unsigned int sum_data_len;
	unsigned long sum_max_delay;
	unsigned long max_delay;
	unsigned long sum_total_delay;
	unsigned long max_total_delay;
	unsigned int max_total_dalay_idx;
	char* maxdelay_component;
	char* max_total_delay_diskname;
	unsigned long* sum_component_delay;
	unsigned long* max_component_delay;
	int count;
};

struct iosdiag_key {
#if 0
	int cpu;
	unsigned long long start_time_ns;
//	unsigned long long io_start_time_ns;
#endif
	unsigned int dev;
	unsigned long sector;
};

int iosdiag_init(char *module_name, unsigned int attach_interval);
int iosdiag_run(int timeout, int mode, char *output_file);
void iosdiag_exit(char *module_name);
#endif
