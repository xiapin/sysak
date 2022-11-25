#include "tsar.h"

#define SCHEDSTAT_PATH "/proc/schedstat"

static char *scheddelay_usage = "    --scheddelay            Per cpu schedule delay (delay time and count)";

struct stats_scheddelay {
	char cpu_name[8];		/* strlen("cpu1024")=7 */
	unsigned long long yld_count;
	unsigned long long sched_count;
	unsigned long long sched_goidle;
	unsigned long long ttwu_count;
	unsigned long long ttwu_local;
	unsigned long long rq_cpu_time;
	unsigned long long run_delay;
	unsigned long long pcount;
};

#define NR_SCHEDDELAY_INFO sizeof(scheddelay_info)/sizeof(struct mod_info)
#define SCHEDDELAY_PERCPU_SIZE (sizeof(struct stats_scheddelay))

unsigned long long value[8];
static void
read_scheddelay_stats(struct module *mod)
{
	int n, pos = 0;
	FILE *fp;
	char line[LEN_1M];
	char buf[LEN_1M];
	struct stats_scheddelay  st;

	memset(buf, 0, LEN_1M);
	memset(&st, 0, SCHEDDELAY_PERCPU_SIZE);
	if ((fp = fopen(SCHEDSTAT_PATH, "r")) == NULL) {
		return;
	}
	while (fgets(line, LEN_1M, fp) != NULL) {
		if (!strncmp(line, "cpu", 3)) {
			n = sscanf(line, "%s %llu %llu %llu %llu %llu %llu %llu %llu %llu",
				st.cpu_name, &st.yld_count, &value[0], &value[1], &value[2],
				&value[3], &value[4], &value[5], &value[6], &value[7]);
			if (n == 9) {
				st.sched_count = value[0];
				st.sched_goidle = value[1];
				st.ttwu_count = value[2];
				st.ttwu_local = value[3];
				st.rq_cpu_time = value[4];
				st.run_delay = value[5];
				st.pcount = value[6];
			} else if (n == 10) {
				st.sched_count = value[1];
				st.sched_goidle = value[2];
				st.ttwu_count = value[3];
				st.ttwu_local = value[4];
				st.rq_cpu_time = value[5];
				st.run_delay = value[6];
				st.pcount = value[7];
			}
			pos += snprintf(buf + pos, LEN_1M - pos,
				"%s=%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu" ITEM_SPLIT,
				st.cpu_name, st.yld_count, st.sched_count,
				st.sched_goidle,st.ttwu_count, st.ttwu_local,
				st.rq_cpu_time,st.run_delay, st.pcount);
			if (strlen(buf) == LEN_1M - 1) {
				fclose(fp);
				return;
			}
		}
	}
	set_mod_record(mod, buf);
	fclose(fp);
	return;
}

static void
set_scheddelay_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
	st_array[0] = cur_array[0] - pre_array[0];
	st_array[1] = cur_array[1] - pre_array[1];
	st_array[2] = cur_array[2] - pre_array[2];
	st_array[3] = cur_array[3] - pre_array[3];
	st_array[4] = cur_array[4] - pre_array[4];
	st_array[5] = cur_array[5] - pre_array[5];
	st_array[6] = cur_array[6] - pre_array[6];
	st_array[7] = cur_array[7] - pre_array[7];
}

static struct mod_info scheddelay_info[] = {
	{"  yld", HIDE_BIT,  0,  STATS_NULL},	/* yld_count */
	{" schd", DETAIL_BIT,  0,  STATS_NULL},	/* sched_count */
	{" idle", HIDE_BIT,  0,  STATS_NULL},	/* sched_goidle */
	{" ttwc", DETAIL_BIT,  0,  STATS_NULL},	/* ttwu_count */
	{" ttwl", HIDE_BIT,  0,  STATS_NULL},	/* ttwu_local */
	{" rqtm", HIDE_BIT,  0,  STATS_NULL},	/* rq_cpu_time */
	{"delay", DETAIL_BIT,  MERGE_ITEM,  STATS_NULL},	/* run_delay */
	{" pcnt", DETAIL_BIT,  MERGE_ITEM,  STATS_NULL},	/* pcount */
};

char *scheddelay_lable = "cpu";

void
mod_register(struct module *mod)
{
	mod->lable = scheddelay_lable;
	register_mod_fields(mod, "--scheddelay", scheddelay_usage, scheddelay_info,
		    NR_SCHEDDELAY_INFO, read_scheddelay_stats, set_scheddelay_record);
}
