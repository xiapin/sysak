#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

typedef int (*INJ_FUNC)(void *args);
struct inj_op {
	const char *inj_name;
	INJ_FUNC inject;
	INJ_FUNC check_result;
};

int inject_oops(void *args);
int inject_panic(void *args);
int inject_hung_task_panic(void *args);
int inject_high_load(void *args);
int inject_high_sys(void *args);
int inject_softlockup(void *args);
int inject_taskhang(void *args);
int inject_taskloop(void *args);
int inject_tasklimit(void *args);
int inject_fdlimit(void *args);
int inject_oom(void *args);
int inject_packdrop(void *args);
int inject_runqlat(void *args);
int inject_iolat(void *args);
int inject_netlat(void *args);

int check_high_load(void *args);
int check_high_sys(void *args);
int check_softlockup(void *args);
int check_taskhang(void *args);
int check_taskloop(void *args);
int check_tasklimit(void *args);
int check_fdlimit(void *args);
int check_oom(void *args);
int check_packdrop(void *args);
int check_runqlat(void *args);
int check_iolat(void *args);
int check_netlat(void *args);

struct inj_op inj_ops[] = {
	{"oops", inject_oops, NULL},
	{"panic", inject_panic, NULL},
	{"hung_task_panic", inject_hung_task_panic, NULL},
	{"high_load", inject_high_load, check_high_load},
	{"high_sys", inject_high_sys, check_high_sys},
	{"softlockup", inject_softlockup, check_softlockup},
	{"taskhang", inject_taskhang, check_taskhang},
	{"taskloop", inject_taskloop, check_taskloop},
	{"tasklimit", inject_tasklimit, check_tasklimit},
	{"fdlimit", inject_fdlimit, check_fdlimit},
	{"oom", inject_oom, check_oom},
	{"packdrop", inject_packdrop, check_packdrop},
	{"runqlat", inject_runqlat, check_runqlat},
	{"iolat", inject_iolat, check_iolat},
	{"netlat", inject_netlat, check_netlat},
};

#define NUM_INJ_OPS (sizeof(inj_ops)/sizeof(struct inj_op))

static char *work_path;

static void usage(void)
{
	int i;

	printf("list for problem injection:\n");
	for (i = 0; i < NUM_INJ_OPS; i++) {
		printf("  %s\n", inj_ops[i].inj_name);
	}
}

static void get_work_path(void)
{
	work_path = getenv("SYSAK_WORK_PATH");
}

static int exec_extern_tool(const char *name, const char *arg)
{
	char filepath[256];
	if (!work_path)
		return -1;

	snprintf(filepath, 255, "%s/tools/%s", work_path, name);
	filepath[255] = 0;
	if (access((filepath), X_OK)) {
		printf("file %s not exist\n",filepath);
		return -1;
	}

	if (arg)
		snprintf(filepath, 255, "bash -c \"%s/tools/%s %s\" &", work_path, name, arg);
	else
		snprintf(filepath, 255, "bash -c %s/tools/%s &", work_path, name);

	filepath[255] = 0;
	return system(filepath);
}

int inject_oops(void *args)
{
	int pid = fork();

	if (pid < 0) {
		return -1;
	} else if (pid == 0) {
		/*wait for parent return*/
		sleep(2);
		system("echo c > /proc/sysrq-trigger");
		return 0;
	}

	return 0;
}

int inject_panic(void *args)
{
        return 0;
}

int inject_hung_task_panic(void *args)
{
        return 0;
}

int inject_high_load(void *args)
{
        return 0;
}

int inject_high_sys(void *args)
{
        return exec_extern_tool("high_sys", NULL);
}

int inject_softlockup(void *args)
{
        return 0;
}

int inject_taskhang(void *args)
{
        return 0;
}

int inject_taskloop(void *args)
{
        return 0;
}

static int get_system_process_limit(void)
{
	int pid_max = -1;
	int threads_max;
	FILE *fp;
	char buf[256];

	fp = fopen("/proc/sys/kernel/pid_max", "r");
	if (!fp) {
		printf("open pid_max failed\n");
		return -1;
	}

	memset(buf, 0, 256);
	if (fgets(buf, 256, fp)) {
		pid_max = atoi(buf);
	}

	fclose(fp);
	if (pid_max < 0)
		return -1;

	fp = fopen("/proc/sys/kernel/threads-max", "r");
	if (!fp) {
		printf("open threads-max failed\n");
		return -1;
	}

	memset(buf, 0, 256);
	if (fgets(buf, 256, fp)) {
		threads_max = atoi(buf);
	}

	fclose(fp);
	if (threads_max < 0)
		return -1;

	return pid_max < threads_max ? pid_max: threads_max;
}

static int get_threads_nr(void)
{
	FILE *fp;
	char buf[256];
	int threads, dummy, ret;
	float fdummy;

	fp = fopen("/proc/loadavg", "r");
	if (!fp)
		return -1;

	fgets(buf, 256, fp);
	ret = sscanf(buf, "%f %f %f %d/%d %d", &fdummy, &fdummy, &fdummy, &dummy, &threads, &dummy);

	if (ret < 1)
		return -1;

	return threads;
}


int inject_tasklimit(void *args)
{
	char buff[32];
	int task_nr = get_system_process_limit() - get_threads_nr();

	if (task_nr > 0) {
		snprintf(buff, 32, "%d", task_nr);
		args = buff;
	} else
		args = NULL;
        return exec_extern_tool("process_limit", args);
}

int inject_fdlimit(void *args)
{
        return 0;
}

int inject_oom(void *args)
{
	return exec_extern_tool("goom", NULL);
}

int inject_packdrop(void *args)
{
        return 0;
}

int inject_runqlat(void *args)
{
        return 0;
}

int inject_iolat(void *args)
{
        return 0;
}

int inject_netlat(void *args)
{
        return 0;
}

int check_high_load(void *args)
{
        return 0;
}

struct proc_stat {
	unsigned long user;
	unsigned long nice;
	unsigned long sys;
	unsigned long idle;
	unsigned long iowait;
	unsigned long irq;
	unsigned long softirq;
};

static int read_proc_stat(struct proc_stat *stat)
{
	FILE *fp;
	char buf[256];
	int ret;

	fp = fopen("/proc/stat", "r");
	if (!fp)
		return -1;

	fgets(buf, 256, fp);
	ret = sscanf(buf, "cpu  %lu %lu %lu %lu %lu %lu %lu",
		&stat->user, &stat->nice, &stat->sys, &stat->idle,
		&stat->iowait, &stat->irq, &stat->softirq);

	if (ret < 7)
		return -1;

	return 0;
}

static int calculate_sys_util(void)
{
	struct proc_stat stat1, stat2;
	unsigned long total = 0, sys;
	float sys_util;

	if (read_proc_stat(&stat1))
		return -1;
	sleep(1);
	if (read_proc_stat(&stat2))
		return -1;

	sys = stat2.sys - stat1.sys;

	total += stat2.user - stat1.user;
	total += stat2.nice - stat1.nice;
	total += sys;
	total += stat2.idle - stat1.idle;
	total += stat2.iowait - stat1.iowait;
	total += stat2.irq - stat1.irq;
	total += stat2.softirq - stat1.softirq;

	sys_util = (float)sys / total;
	return (int)(sys_util*100);
}

int check_high_sys(void *args)
{
	int i;

	for (i = 0; i < 5; i++) {
		if (calculate_sys_util() > 5)
			return 0;
		sleep(1);
	}

	return -1;
}

int check_softlockup(void *args)
{
        return 0;
}

int check_taskhang(void *args)
{
        return 0;
}

int check_taskloop(void *args)
{
        return 0;
}

int check_tasklimit(void *args)
{
	int i, limit;

	limit = get_system_process_limit();
	if (limit < 0)
		return -1;

	limit -= 100;
	for (i = 0; i < 20; i++) {
		if (get_threads_nr() > limit)
			return 0;
		sleep(1);
	}

	return -1;
}

int check_fdlimit(void *args)
{
        return 0;
}

int check_oom(void *args)
{
        return 0;
}

int check_packdrop(void *args)
{
        return 0;
}

int check_runqlat(void *args)
{
        return 0;
}

int check_iolat(void *args)
{
        return 0;
}

int check_netlat(void *args)
{
        return 0;
}

int main(int argc, char *argv[])
{
	int ret = -EINVAL, i;
	char *args = NULL;

	if (argc < 2)
		return ret;

	if (argc == 2 && !strcmp(argv[1], "-h")) {
		usage();
		return 0;
	}

	get_work_path();

	if (argc == 3)
		args = argv[2];

	for (i = 0; i < NUM_INJ_OPS; i++) {
		if (!strcmp(argv[1], inj_ops[i].inj_name)) {
			ret = inj_ops[i].inject(args);
			if (ret == 0){
				if (inj_ops[i].check_result)
					ret = inj_ops[i].check_result(args);
			}
 
			break;
		}
	}

	if (ret) {
		if (i >= NUM_INJ_OPS)
			printf("Invalid inputs\n");

		printf("Failed\n");
	} else {
		printf("Done\n");
	}

	return 0;
}

