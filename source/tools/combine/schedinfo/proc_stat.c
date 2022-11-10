#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <linux/types.h> /* for __u64 __u32 */
#include "schedinfo.h"

extern unsigned int nr_cpus;

static inline bool test_state(char *str, char ch)
{
	return !!strchr(str, ch);
}

static inline bool state_contribute_load(char *str)
{
	return ((!!strchr(str, 'D')) || (!!strchr(str, 'R')));
}

int record_logs(void)
{
	return 0;
}

int get_task_stack(long tid, FILE *outf)
{
	FILE *result;
	char path[32] = {0};
	char buffer[512] = {0};

	snprintf(path, sizeof(path),
		"/proc/%ld/stack", tid);
	result = fopen(path, "r");
	if (!result)
		return errno;

	while(fgets(buffer, sizeof(buffer), result)) {
		fprintf(outf, "%s", buffer);
	}

	return 0;
}

int init_top_struct(struct top_utils *top, char *prefix)
{
	struct tm *nowdt;
	time_t t = time(NULL);
	char file_path[512], ymd[32];

	nowdt = localtime(&t);	/* localtime() return a static pointer, so we can't use it in struct. */
	snprintf(ymd, sizeof(ymd), "%d-%02d-%02d",
		nowdt->tm_year+1900, nowdt->tm_mon, nowdt->tm_mday);

	snprintf(file_path, sizeof(file_path),
		"%s/%s.schedinfo.log", prefix, ymd);
	top->log = (void *)(fopen(file_path, "a"));
	if (!top->log)
		return errno;
	top->last_dt = *nowdt;

	return 0;
}

FILE *check_log_file(struct top_utils *top, char *prefix, char *ymd, char *hms, struct tm *nowdt)
{
	FILE *oldf, *tmpf;
	struct tm *olddt;
	char file_path[64];

	olddt = &top->last_dt;

	oldf = (FILE *)(top->log);
	if (!oldf) {
		snprintf(file_path, sizeof(file_path),
			"%s/%s.schedinfo.log", prefix, ymd);
		top->log = (void *)(fopen(file_path, "a"));
		/* localtime() return a static pointer, so we can't use it in struct. */
		top->last_dt = *nowdt;
		goto out;
	}

	if (olddt->tm_mday != nowdt->tm_mday) {
		snprintf(file_path, sizeof(file_path),
			"%s/%s.schedinfo.log", prefix, ymd);
		tmpf = fopen(file_path, "a");
		if (!tmpf) {
			fprintf(oldf, "[%s %s] fopen(%s) fail\n", ymd, hms, file_path);
		} else {
			fclose(oldf);
			top->log = tmpf;
			top->last_dt = *nowdt;
		}
	}
out:
	return (FILE *)(top->log);
}

/*
 *TODO: crond to rotate the log files; e.g: only keep last 30 days log files
 * */
int record_top_util_proces(struct top_utils *top)
{
	int i = 0;
	struct tm *nowdt;
	FILE *result, *logf;
	struct ps_info ps;
	char cmd[128] = {0};
	char buffer[1024];
	char ymd[32], hms[32];
	time_t t = time(NULL);

	snprintf(cmd, sizeof(cmd),
		"ps -e -T -o pid,tid,pcpu,etime,time,state,comm | sort -k 3 -r -g");
	result = popen(cmd, "r");
	if (feof(result))
		return -1;

	i = 0;
	nowdt = localtime(&t);	/* localtime() return a static pointer, so we can't use it in struct. */
	snprintf(ymd, sizeof(ymd), "%d-%02d-%02d",
		nowdt->tm_year+1900, nowdt->tm_mon, nowdt->tm_mday);
	snprintf(hms, sizeof(hms), "%02d:%02d:%02d",
		nowdt->tm_hour, nowdt->tm_min, nowdt->tm_sec);
	logf = check_log_file(top, SYSAK_LOG_PATH, ymd, hms, nowdt);
	if (!logf)
		logf = stdout;

	/*
 	 *Record the topN cpu% tasks and D state tasks
	 *tgid tid util etime time state comm [stack]
 	 * */
	fprintf(logf, "%s %s\n", ymd, hms);
	while(fgets(buffer, sizeof(buffer), result)) {
		sscanf(buffer, "%ld %ld %f %s %s %s %s", 
			&ps.pid, &ps.tid, &ps.cpu, ps.etime, 
			ps.time, ps.state, ps.comm);
		if ((i < nr_cpus) || test_state(ps.state, 'R')) {
			fprintf(logf, "%ld, %ld, %4.2f, %s, %s, %s, %s\n", 
				ps.pid, ps.tid, ps.cpu, ps.etime,
				ps.time, ps.state, ps.comm);
		}
		if (test_state(ps.state, 'D')) {
			fprintf(logf, "%ld, %ld, %4.2f, %s, %s, %s, %s\n", 
				ps.pid, ps.tid, ps.cpu, ps.etime,
				ps.time, ps.state, ps.comm);
			get_task_stack(ps.tid, logf);
		}
		i++;
	}
	return 0;
}

bool fire_warn(struct schedinfo *schedinfo)
{
	/*
 	*TODO: How can we define the when to fire the action to
	* record the top cpu% and D state processes
 	* */
	__u32 warnbits = schedinfo->warnbits;

	if (warnbits)
		return true;
	else
		return false;
}

/*
 *TODO
 *1) export datas to promethus
 *2) judge which is the burst
 * */
int parse_proc_stat(struct schedinfo *schedinfo)
{
	int i;
	__u64 total;
	FILE *fp;
	char line[4096];
	struct sched_datas *tmp;
	struct sched_datas *datas, *prev;

	if ((fp = fopen(STAT_PATH, "r")) == NULL)
		return 0;
	i = 0;

	/* 
 	* switch the prev and now pointer.
 	* This can less memory copy.
 	* */
	tmp = schedinfo->datass;
	schedinfo->datass = schedinfo->prev;
	schedinfo->prev = tmp;

	tmp = schedinfo->allcpu;
	schedinfo->allcpu = schedinfo->allcpuprev;
	schedinfo->allcpuprev = tmp;
	while (fgets(line, 4096, fp) != NULL) {
		if (!strncmp(line, "cpu", 3)) {
			struct sched_datas *datas, *prev;
			if (!strncmp(line, "cpu ", 4)) {
				datas = schedinfo->allcpu;
				prev = schedinfo->allcpuprev;
			} else {
				datas = &schedinfo->datass[i];
				prev = &schedinfo->prev[i++];
			}
			sscanf(line, "%s %llu %llu %llu %llu %llu %llu %llu %llu %llu",
				datas->cpuid, &datas->user, &datas->nice, &datas->sys,
				&datas->idle,&datas->iowait, &datas->hirq, &datas->sirq,
				&datas->steal, &datas->guest);
			datas->total = (datas->user + datas->nice + datas->sys + datas->iowait +
				 datas->hirq + datas->sirq + datas->steal + datas->guest + datas->idle - 
				 prev->user - prev->nice - prev->sys - prev->iowait -
				 prev->hirq - prev->sirq - prev->steal - prev->guest - prev->idle);
		} else if (!strncmp(line, "processes ", 10)) {
			sscanf(line + 10, "%llu", &schedinfo->nr_forked);
		} else if (!strncmp(line, "procs_running ", 14)) {
			sscanf(line + 14, "%llu", &schedinfo->allcpu->nr_running);
		} else if (!strncmp(line, "procs_blocked ", 14)) {
			sscanf(line + 14, "%llu", &schedinfo->nr_block);
		}
	}
	i = 0;
	datas = schedinfo->allcpu;
	prev = schedinfo->allcpuprev;
	do {
		total = datas->total;
		if (total <= 0) {
			datas = &schedinfo->datass[i];
			prev = &schedinfo->prev[i];
			i++;
			continue;
		}
		datas->user_util = (double)(datas->user - prev->user) / total;
		datas->sys_util = (double)(datas->sys - prev->sys) / total;
		datas->nice_util = (double)(datas->nice - prev->nice) / total;
		datas->iowait_util = (double)(datas->iowait - prev->iowait) / total;
		datas->hirq_util = (double)(datas->hirq - prev->hirq) / total;
		datas->sirq_util = (double)(datas->sirq - prev->sirq) / total;
		datas->idle_util = (double)(datas->idle - prev->idle) / total;
		if (i >= nr_cpus)
			break;
		datas = &schedinfo->datass[i];
		prev = &schedinfo->prev[i];
		i++;
	} while (i <= nr_cpus);

	if (fire_warn(schedinfo))
		record_top_util_proces(&schedinfo->top);

	fclose(fp);
	return 0;
}

