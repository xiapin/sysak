#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "proc_schedstat.h"

#define LINE_LEN	128
#define SCHEDSTAT_PATH	"/proc/schedstat"

struct sched_stats {
	char cpu_name[8];		/* strlen("cpu1024")=7 */
	unsigned long long yld_count;
	unsigned long long sched_count;
	unsigned long long sched_goidle;
	unsigned long long ttwu_count;
	unsigned long long ttwu_local;
	unsigned long long rq_cpu_time;
	unsigned long long delay;
	unsigned long long pcount;
};

long nr_cpus;
char *real_proc_path;
struct unity_line **lines1;
struct sched_stats curr_min, curr_max;
struct sched_stats *schstats, *schstats2, *delta, *curr, *oldp;

int init(void * arg)
{
	int ret;
	int i, lenth;
	char *mntpath = get_unity_proc();

	lenth = strlen(mntpath)+strlen(SCHEDSTAT_PATH);
	real_proc_path = calloc(lenth+2, 1);
	if (!real_proc_path)
		return -errno;
	snprintf(real_proc_path, lenth+1, "%s%s", mntpath, SCHEDSTAT_PATH);
	printf("path=%s\n", real_proc_path);
	errno = 0;

	lines1 = NULL;
	schstats = schstats2 = delta = curr = oldp = NULL;
	nr_cpus = sysconf(_SC_NPROCESSORS_CONF);
	if (nr_cpus < 0) {
		ret = errno;
		printf("WARN: proc_schedstat install FAIL sysconf\n");
		return ret;
	}

	errno = 0;
	schstats = calloc(sizeof(struct sched_stats), nr_cpus);
	if (!schstats) {
		ret = errno;
		printf("WARN: proc_schedstat install FAIL calloc 1\n");
		return ret;
	}
	schstats2 = calloc(sizeof(struct sched_stats), nr_cpus);
	if (!schstats2) {
		ret = errno;
		printf("WARN: proc_schedstat install FAIL calloc 2\n");
		return ret;
	}
	delta = calloc(sizeof(struct sched_stats), nr_cpus);
	if (!delta) {
		ret = errno;
		printf("WARN: proc_schedstat install FAIL calloc 3\n");
		return ret;
	}
	curr = schstats;
	oldp = schstats2;
	lines1 = calloc(sizeof(struct unity_line *), nr_cpus);
	if (!lines1) {
		ret = errno;
		printf("WARN: proc_schedstat install FAIL calloc 4\n");
		return ret;
	}

	curr_min.delay = -1ULL;
	curr_max.delay = 0;
	printf("proc_schedstat plugin install.\n");
	return 0;
}

static void gen_delta(struct sched_stats *curr, struct sched_stats *old, struct sched_stats *delta)
{
	delta->pcount = curr->pcount - old->pcount;
	delta->delay = curr->delay - old->delay;
}

int full_line(struct unity_line **uline1, struct unity_line *uline2)
{
	int n, i, ret, idx;
	long cpu;
	FILE *fp;
	char line[128], cpu_name[8];
	struct sched_stats *st, *tmp;
	unsigned long long value[8] = {0};
	unsigned long long sum_delay, sum_cnt;

	fp = NULL;
	errno = 0;
	idx = 0;
	if ((fp = fopen(real_proc_path, "r")) == NULL) {
		ret = errno;
		printf("WARN: proc_schedstat install FAIL fopen\n");
		return ret;
	}

	sum_delay = sum_cnt = 0;
	memset(line, 0, sizeof(line));
	while (fgets(line, LINE_LEN, fp) != NULL) {
		if (!strncmp(line, "cpu", 3)) {
			n = sscanf(line+3, "%ld", &cpu);
			if (n == 1 && cpu >= 0 && cpu < nr_cpus) {
				st = &curr[cpu];
			} else {
				printf("WARN:sscanf/cpu fails... n=%d,cpu=%ld, nr_cpu=%ld\n", n, cpu, nr_cpus);
				printf("line=[%s]\n", line+3);
				continue;
			}
			memset(cpu_name, 0, sizeof(cpu_name));
			n = sscanf(line, "%s %llu %llu %llu %llu %llu %llu %llu %llu %llu",
				cpu_name, &st->yld_count, &value[0], &value[1], &value[2],
				&value[3], &value[4], &value[5], &value[6], &value[7]);
			if (n == 9) {
				st->sched_count = value[0];
				st->sched_goidle = value[1];
				st->ttwu_count = value[2];
				st->ttwu_local = value[3];
				st->rq_cpu_time = value[4];
				st->delay = value[5];
				st->pcount = value[6];
			} else if (n == 10) {
				st->sched_count = value[1];
				st->sched_goidle = value[2];
				st->ttwu_count = value[3];
				st->ttwu_local = value[4];
				st->rq_cpu_time = value[5];
				st->delay = value[6];
				st->pcount = value[7];
			}
			gen_delta(st, &oldp[cpu], &delta[cpu]);
#if PROC_SCH_DEBUG
			printf("%s: pcount=%llu, delay=%llu\n",
				cpu_name, delta[cpu].pcount, delta[cpu].delay);
#endif
			unity_set_index(uline1[cpu], 0, "cpu", cpu_name);
			unity_set_value(uline1[cpu], 0, "pcount", delta[cpu].pcount);
			unity_set_value(uline1[cpu], 1, "delay", delta[cpu].delay);
			if (delta[cpu].delay > curr_max.delay) {
				curr_max.delay = delta[cpu].delay;
				curr_max.pcount = delta[cpu].pcount;
				strcpy(curr_max.cpu_name, cpu_name);
			}
			if (delta[cpu].delay < curr_min.delay) {
				curr_min.delay = delta[cpu].delay;
				curr_min.pcount = delta[cpu].pcount;
				strcpy(curr_min.cpu_name, cpu_name);
			}
		}
	}

	/* The avg of ALL cpus */
	for (i = 0; i < nr_cpus; i++) {
		sum_cnt += delta[i].pcount;
		sum_delay += delta[i].delay;
	}
#if PROC_SCH_DEBUG
	printf("avg: pcount=%llu, delay=%llu\n",
		sum_cnt/nr_cpus, sum_delay/nr_cpus);
#endif
	unity_set_index(uline2, 0, "summary", "avg");
	unity_set_value(uline2, 0, "pcount", sum_cnt/nr_cpus);
	unity_set_value(uline2, 1, "delay", sum_delay/nr_cpus);

	tmp = curr;
	curr = oldp;
	oldp = tmp;
	if (fp)
		fclose(fp);
}

int call(int t, struct unity_lines* lines) {
	int i = 0;
	static double value = 0.0;
	struct unity_line *line2, *line3, *line4;

	unity_alloc_lines(lines, nr_cpus+3);
	for (i = 0; i < nr_cpus; i++) {
		lines1[i] = unity_get_line(lines, i);
		unity_set_table(lines1[i], "proc_schedstat");
	}
	line2 = unity_get_line(lines, nr_cpus);
	unity_set_table(line2, "proc_schedstat");
	full_line(lines1, line2);

	line3 = unity_get_line(lines, nr_cpus+1);
	unity_set_table(line3, "proc_schedstat");
	unity_set_index(line3, 0, "max", curr_max.cpu_name);
	unity_set_value(line3, 0, "pcount", curr_max.delay);
	unity_set_value(line3, 1, "delay", curr_max.pcount);

	line4 = unity_get_line(lines, nr_cpus+2);
	unity_set_table(line4, "proc_schedstat");
	unity_set_index(line4, 0, "min", curr_min.cpu_name);
	unity_set_value(line4, 0, "pcount", curr_min.delay);
	unity_set_value(line4, 1, "delay", curr_min.pcount);
	return 0;
}

void deinit(void)
{
	if (schstats)
		free(schstats);
	if (schstats2)
		free(schstats2);
	if (delta)
		free(delta);
	if (lines1)
		free(lines1);
	printf("proc_schedstat plugin uninstall\n");
}
