#include "tsar.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <linux/types.h> /* for __u64 __u32 */

/*
 * This is a "non-standard" warn trigger example.
 * When a warn triggered by fire_warn_func() in collect_record_stat(), 
 * warn_record_thread() will be waked up and dispatch it to its belong 
 *
 * */

unsigned int gnr_cpus;

static inline bool test_state(char *str, char ch)
{
	return !!strchr(str, ch);
}

static inline bool state_contribute_load(char *str)
{
	return ((!!strchr(str, 'D')) || (!!strchr(str, 'R')));
}

int get_task_stack(long tid, FILE *outf)
{
	size_t len = 0;
	FILE *result;
	char path[32] = {0};
	char buffer[512] = {0};

	snprintf(path, sizeof(path),
		"/proc/%ld/stack", tid);
	result = fopen(path, "r");
	if (!result)
		return 0;

	while(fgets(buffer, sizeof(buffer), result)) {
		len += fprintf(outf, "%s", buffer);
	}

	return len;
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
	size_t len = 0;
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

	logf = check_log_file(top, conf.output_dictory, ymd, hms, nowdt);
	if (!logf)
		logf = stdout;
	/*
 	 *Record the topN cpu% tasks and D state tasks
	 *tgid tid util etime time state comm [stack]
 	 * */
	len += fprintf(logf, "%s %s\n", ymd, hms);
	while(fgets(buffer, sizeof(buffer), result)) {
		sscanf(buffer, "%ld %ld %f %s %s %s %s", 
			&ps.pid, &ps.tid, &ps.cpu, ps.etime, 
			ps.time, ps.state, ps.comm);
		if ((i < gnr_cpus) || test_state(ps.state, 'R')) {
			len += fprintf(logf, "%ld, %ld, %4.2f, %s, %s, %s, %s\n", 
				ps.pid, ps.tid, ps.cpu, ps.etime,
				ps.time, ps.state, ps.comm);
		}
		if (test_state(ps.state, 'D')) {
			len += fprintf(logf, "%ld, %ld, %4.2f, %s, %s, %s, %s\n", 
				ps.pid, ps.tid, ps.cpu, ps.etime,
				ps.time, ps.state, ps.comm);
			len += get_task_stack(ps.tid, logf);
		}
		i++;
	}
	fflush(logf);

	/* printf("record_top_util_proces:write %d bytes to log file\n", len); */
	return len;
}

