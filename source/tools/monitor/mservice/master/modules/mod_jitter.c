/*
 * iostat.c for 2.6.* with file /proc/diskstat
 * Linux I/O performance monitoring utility
 */
#include "tsar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <sys/param.h>
#include <linux/major.h>

#define NR_MODS	3
#define BUF_SIZE	(NR_MODS * 4096)
char g_buf[BUF_SIZE];
char line[512];
static int jitter_init = 0;
char *jitter_usage = "    --jit                Application jitter stats";
char *jit_mod[] = {"rqslow", "noschd", "irqoff"};
char *log_path[] = {
	"/var/log/sysak/mservice/runqslower",
	"/var/log/sysak/mservice/nosched",
	"/var/log/sysak/mservice/irqoff",
};

struct summary {
	unsigned long num;
	unsigned long long total;
	int lastcpu0, lastcpu1, lastcpu2, lastcpu3;
	unsigned long lastjit0, lastjit1, lastjit2, lastjit3;
};

static struct mod_info jitter_info[] = {
	{"   num", HIDE_BIT,  0,  STATS_NULL},		/* total numbers of happend */
	{"  time", HIDE_BIT,  0,  STATS_NULL},		/* the sum-time of delay */
	{" lCPU0", DETAIL_BIT,  0,  STATS_NULL},	/* last happened cpu[0] */
	{" lCPU1", DETAIL_BIT,  0,  STATS_NULL},	/* last happened cpu[1] */
	{" lCPU2", DETAIL_BIT,  0,  STATS_NULL},	/* last happened cpu[2] */
	{" lCPU3", DETAIL_BIT,  0,  STATS_NULL},	/* last happened cpu[3] */
	{"dltnum", SUMMARY_BIT,  0,  STATS_NULL},	/* delta numbers of happend */
	{" dlttm", SUMMARY_BIT,  0,  STATS_NULL},	/* the delta time of delay */
};

#define NR_JITTER_INFO sizeof(jitter_info)/sizeof(struct mod_info)
struct summary summary;

int prepare_jitter_dictory(char *path)
{
	int ret;

	ret = mkdir(path, 0777);
	if (ret < 0 && errno != EEXIST)
		return errno;
	else
		return 0;
}

int init_sysak(void)
{
	int ret;
	FILE *fp1, *fp2, *fp3;
	char *mservice_log_dir = "/var/log/sysak/mservice/";

	if (jitter_init)
		return 0;

	ret = prepare_jitter_dictory(mservice_log_dir);
	if (ret)
		return ret;

	/* todo: what if command can't be find? */
	/* threshold is 40ms */
	fp1 = popen("sysak runqslower -S -f /var/log/sysak/mservice/runqslower 40 2>/dev/null &", "r");
	if (!fp1) {
		perror("popen runqslower");
		return -1;
	}

	fp2 = popen("sysak nosched -S -f /var/log/sysak/mservice/nosched -t 10 2>/dev/null &", "r");
	if (!fp2) {
		perror("popen nosched");
		return -1;
	}

	fp3 = popen("sysak irqoff -S -f /var/log/sysak/mservice/irqoff 10 2>/dev/null &", "r");
	if (!fp3) {
		perror("popen irqoff");
		return -1;
	}
	jitter_init = 1;
	return 0;
}

static int get_jitter_info(char *path, struct summary *sump)
{
	int ret = -1;
	FILE *fp;

	if((fp = fopen(path, "r")) == NULL) {
		fprintf(stderr, "fopen %s fail\n", path);
		return ret;
	}
	memset(line, 0, sizeof(line));
	if (fgets(line, sizeof(line), fp) != NULL) {
		/* "irqoff", "noschd", "rqslow" has 6 charactors */
		sscanf(line+6, "%lu %llu %d %d %d %d %lu %lu %lu %lu",
			&sump->num, &sump->total,
			&sump->lastcpu0, &sump->lastcpu1,
			&sump->lastcpu2, &sump->lastcpu3,
			&sump->lastjit0, &sump->lastjit1,
			&sump->lastjit2, &sump->lastjit3);
		ret = 0;
	} else {
		fprintf(stderr, "fgets %s fail:%s\n", path, strerror(errno));
	}
	rewind(fp);
	fclose(fp);
	return ret;
}

void print_jitter_stats(struct module *mod)
{
	int i, ret, pos;

	pos = 0;
	memset(g_buf, 0, BUF_SIZE);
	for (i = 0; i < NR_MODS; i++) {
		memset(&summary, 0, sizeof(struct summary));
		ret = get_jitter_info(log_path[i], &summary);
		if (ret < 0)
			continue;
		pos += snprintf(g_buf + pos, BUF_SIZE - pos, "%s=%ld,%llu,%d,%d,%d,%d,%lu,%lu,%lu,%lu,%d" ITEM_SPLIT,
			jit_mod[i], summary.num, summary.total,
			summary.lastcpu0, summary.lastcpu1,
			summary.lastcpu2, summary.lastcpu3,
			summary.lastjit0, summary.lastjit1,
			summary.lastjit2, summary.lastjit3, pos);
	}
	set_mod_record(mod, g_buf);
}

void read_jitter_stat(struct module *mod, char *parameter)
{
	int ret;
	ret = init_sysak();
	if (ret)
		fprintf(stderr, "init_sysak failed\n");/*todo*/

	print_jitter_stats(mod);
}

static void
set_jitter_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
	st_array[0] = cur_array[0];
	st_array[1] = cur_array[1];
	st_array[2] = cur_array[2];
	st_array[3] = cur_array[3];
	st_array[4] = cur_array[4];
	st_array[5] = cur_array[5];

	if (cur_array[0] >= pre_array[0])
		st_array[6] = cur_array[0] - pre_array[0];
	else
		st_array[6] = -1;
	if (cur_array[1] >= pre_array[1])
		st_array[7] = cur_array[1] - pre_array[1];
	else
		st_array[7] = -1;
}

void
mod_register(struct module *mod)
{
	register_mod_fields(mod, "--jit", jitter_usage, jitter_info,
			NR_JITTER_INFO, read_jitter_stat, set_jitter_record);
}
