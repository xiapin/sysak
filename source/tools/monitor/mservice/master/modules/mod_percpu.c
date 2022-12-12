#include "tsar.h"
#include <stdbool.h>

#define STAT_PATH "/proc/stat"

static char *percpu_usage = "    --percpu            Per cpu share (user, system, interrupt, nice, & idle)";

struct stats_percpu {
    unsigned long long cpu_user;
    unsigned long long cpu_nice;
    unsigned long long cpu_sys;
    unsigned long long cpu_idle;
    unsigned long long cpu_iowait;
    unsigned long long cpu_steal;
    unsigned long long cpu_hardirq;
    unsigned long long cpu_softirq;
    unsigned long long cpu_guest;
	unsigned long long nr_running;
    char cpu_name[10];
};

#define STATS_PERCPU_SIZE (sizeof(struct stats_percpu))

static int begine_read_nrrun(U_64 *nr_run, int nr_cpu)
{
#define SCHED_DEBUG	"/proc/sched_debug"
	int nr = 0, idx = 0;
	FILE *fp;
	char line[1024];

	if ((fp = fopen(SCHED_DEBUG, "r")) == NULL) {
		return -errno;
	}

	while (fgets(line, 1024, fp) != NULL) {
		if (!strncmp(line, "cpu#", 4)) {
			sscanf(line + 4, "%d", &idx);
			if (idx > nr_cpu)
				continue;
			if (!fgets(line, 1024, fp))	/* read the next line */
				continue;
			if (!strncmp(line, "  .nr_running", 12)) {
				sscanf(line + 34, "%llu", &nr_run[idx]);
				nr++;
			}
		}
	}

	if (fclose(fp))
		perror("begine_read_nrrun: fclose fail\n");
	return nr;
}

static void
read_percpu_stats(struct module *mod)
{
    int                  pos = 0, cpus = 0;
    FILE                *fp;
    char                 line[LEN_1M];
    char                 buf[LEN_1M];
    struct stats_percpu  st_percpu;
	int nr = 0;
	static U_64 *nr_run = NULL;
	static int nr_cpus = 0;

	if (!nr_cpus)
		nr_cpus = sysconf(_SC_NPROCESSORS_CONF);

	if (!nr_run) {
		nr_run = calloc(nr_cpus, sizeof(U_64));
		if (!nr_run)
			return;
	}

    memset(buf, 0, LEN_1M);
    memset(&st_percpu, 0, STATS_PERCPU_SIZE);
    if ((fp = fopen(STAT_PATH, "r")) == NULL) {
        return;
    }
	memset(nr_run, 0, nr_cpus*sizeof(U_64));
	begine_read_nrrun(nr_run, nr_cpus);
    while (fgets(line, LEN_1M, fp) != NULL) {
        if (!strncmp(line, "cpu", 3)) {
            /*
             * Read the number of jiffies spent in the different modes
             * (user, nice, etc.) among all proc. CPU usage is not reduced
             * to one processor to avoid rounding problems.
             */
            sscanf(line, "%s %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                    st_percpu.cpu_name,
                    &st_percpu.cpu_user,
                    &st_percpu.cpu_nice,
                    &st_percpu.cpu_sys,
                    &st_percpu.cpu_idle,
                    &st_percpu.cpu_iowait,
                    &st_percpu.cpu_hardirq,
                    &st_percpu.cpu_softirq,
                    &st_percpu.cpu_steal,
                    &st_percpu.cpu_guest);
            if (st_percpu.cpu_name[3] == '\0') //ignore cpu summary stat
                continue;
	    st_percpu.nr_running = nr_run[nr++];
            pos += snprintf(buf + pos, LEN_1M - pos, "%s=%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu" ITEM_SPLIT,
                    /* the store order is not same as read procedure */
                    st_percpu.cpu_name,
                    st_percpu.cpu_user,
                    st_percpu.cpu_sys,
                    st_percpu.cpu_iowait,
                    st_percpu.cpu_hardirq,
                    st_percpu.cpu_softirq,
                    st_percpu.cpu_idle,
                    st_percpu.cpu_nice,
                    st_percpu.cpu_steal,
                    st_percpu.cpu_guest,
			st_percpu.nr_running);
            if (strlen(buf) == LEN_1M - 1) {
                fclose(fp);
                return;
            }
            cpus++;
        }
    }
    set_mod_record(mod, buf);
    fclose(fp);
    return;
}

static void
set_percpu_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int    i, j;
    U_64   pre_total, cur_total;
    pre_total = cur_total = 0;

    for (i = 0; i < mod->n_col; i++) {
        if(cur_array[i] < pre_array[i]){
            for(j = 0; j < 9; j++)
                st_array[j] = -1;
            return;
        }
        pre_total += pre_array[i];
        cur_total += cur_array[i];
    }

    /* no tick changes, or tick overflows */
    if (cur_total <= pre_total)
        return;
    /* set st record */
    for (i = 0; i < 9; i++) {
        /* st_array[5] is util, calculate it late */
        if((i != 5) && (cur_array[i] >= pre_array[i]))
            st_array[i] = (cur_array[i] - pre_array[i]) * 100.0 / (cur_total - pre_total);
    }

    /* util = user + sys + hirq + sirq + nice */
    st_array[5] = st_array[0] + st_array[1] + st_array[3] + st_array[4] + st_array[6];
#if 0
	/* this is a example for warn triger */
	{	int idx = (st_array - &mod->st_array[0])/mod->n_col;
		if (st_array[5] > 50) {
			mod->warn_triger = (TRIGER_PERCPU << 32) | idx;
		}
	}
#endif
	st_array[9] = cur_array[9];
}

static struct mod_info percpu_info[] = {
    {"  user", DETAIL_BIT,  MERGE_SUM,  STATS_NULL},
    {"   sys", DETAIL_BIT,  MERGE_SUM,  STATS_NULL},
    {"  wait", DETAIL_BIT,  MERGE_SUM,  STATS_NULL},
    {"  hirq", DETAIL_BIT,  MERGE_SUM,  STATS_NULL},
    {"  sirq", DETAIL_BIT,  MERGE_SUM,  STATS_NULL},
    {"  util", SUMMARY_BIT,  MERGE_SUM,  STATS_NULL},
    {"  nice", HIDE_BIT,  MERGE_SUM,  STATS_NULL},
    {" steal", HIDE_BIT,  MERGE_SUM,  STATS_NULL},
    {" guest", HIDE_BIT,  MERGE_SUM,  STATS_NULL},
    {"nr_run", DETAIL_BIT,  MERGE_SUM,  STATS_NULL},
};

char *percpu_lable = "cpu";

void
mod_register(struct module *mod)
{
	mod->lable = percpu_lable;
    register_mod_fields(mod, "--percpu", percpu_usage, percpu_info, 10, read_percpu_stats, set_percpu_record);
}
