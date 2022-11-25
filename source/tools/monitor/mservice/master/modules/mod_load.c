#include "tsar.h"
#if 0
#define LOAD_DETAIL_HDR(d)          \
    "  runq"d" plist"d"  min1"d     \
"  min5"d" min15"

#define LOAD_STORE_FMT(d)           \
    "%ld"d"%d"d"%d"d"%d"d"%d"
#endif
char *load_usage = "    --load              System Run Queue and load average";

/* Structure for queue and load statistics */
struct stats_load {
    unsigned long nr_running;
    unsigned int  load_avg_1;
    unsigned int  load_avg_5;
    unsigned int  load_avg_15;
    unsigned int  nr_threads;
	unsigned long long nr_io;
	unsigned long long nr_forked;
	long long nr_unint;
};

static struct mod_info load_info[] = {
    {" load1", SUMMARY_BIT,  0,  STATS_NULL},
    {" load5", DETAIL_BIT,  0,  STATS_NULL},
    {"load15", DETAIL_BIT,  0,  STATS_NULL},
    {"  runq", DETAIL_BIT,  0,  STATS_NULL},
    {"  plit", DETAIL_BIT,  0,  STATS_NULL},
    {" nfork", DETAIL_BIT,  0,  STATS_NULL},	/* nr_forked */
    {"  nrio", DETAIL_BIT,  0,  STATS_NULL},	/* nr_io */
    {" unint", DETAIL_BIT,  0,  STATS_NULL}
};

#define NR_LOAD_INFO (sizeof(load_info)/sizeof(struct mod_info))

static void read_proc_stats(struct stats_load *st_load, char *buf)
{
	FILE *fp;
	char line[LEN_4096];

	if ((fp = fopen(STAT, "r")) == NULL)
		return;

	while (fgets(line, LEN_4096, fp) != NULL) {
		if (!strncmp(line, "processes ", 10)) {
			sscanf(line + 10, "%llu", &st_load->nr_forked);
		} else if (!strncmp(line, "procs_blocked ", 14)) {
			sscanf(line + 14, "%llu", &st_load->nr_io);
		}
	}
}

static int read_sched_debug(struct stats_load *st_load, char *buf)
{
#define SCHED_DEBUG	"/proc/sched_debug"
	FILE *fp;
	long long total_unint = 0, nr_unint;;
	char line[1024];

	if ((fp = fopen(SCHED_DEBUG, "r")) == NULL) {
		return -errno;
	}

	while (fgets(line, 1024, fp) != NULL) {
		if (!strncmp(line, "  .nr_uninterruptible", 21)) {
			sscanf(line + 34, "%lld", &nr_unint);
			total_unint += nr_unint;
		}
	}
	if (total_unint < 0)
		total_unint = 0;

	st_load->nr_unint = total_unint;
	return total_unint;
}

void
read_stat_load(struct module *mod)
{
    int     load_tmp[3];
    FILE   *fp;
    char    buf[LEN_4096];
    struct  stats_load st_load;
    memset(buf, 0, LEN_4096);
    memset(&st_load, 0, sizeof(struct stats_load));

	read_proc_stats(&st_load, buf);
	read_sched_debug(&st_load, buf);
    if ((fp = fopen(LOADAVG, "r")) == NULL) {
        return;
    }

    /* Read load averages and queue length */
    if (fscanf(fp, "%d.%d %d.%d %d.%d %ld/%d %*d\n",
            &load_tmp[0], &st_load.load_avg_1,
            &load_tmp[1], &st_load.load_avg_5,
            &load_tmp[2], &st_load.load_avg_15,
            &st_load.nr_running,
            &st_load.nr_threads) != 8)
    {
        fclose(fp);
        return;
    }
    st_load.load_avg_1  += load_tmp[0] * 100;
    st_load.load_avg_5  += load_tmp[1] * 100;
    st_load.load_avg_15 += load_tmp[2] * 100;

    if (st_load.nr_running) {
        /* Do not take current process into account */
        st_load.nr_running--;
    }

    sprintf(buf , "%u,%u,%u,%lu,%u,%llu,%llu,%lld",
            st_load.load_avg_1,
            st_load.load_avg_5,
            st_load.load_avg_15,
            st_load.nr_running,
            st_load.nr_threads,
            st_load.nr_forked,
            st_load.nr_io,
            st_load.nr_unint);
    set_mod_record(mod, buf);
    fclose(fp);
}

static void
set_load_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;
    for (i = 0; i < 3; i++) {
        st_array[i] = cur_array[i] / 100.0;
    }
    st_array[3] = cur_array[3];
    st_array[4] = cur_array[4];
    st_array[5] = cur_array[5] - pre_array[5];
    st_array[6] = cur_array[6] - pre_array[6];
    st_array[7] = cur_array[7];
}

void
mod_register(struct module *mod)
{
	mod->lable = NULL;
    register_mod_fields(mod, "--load", load_usage, load_info, NR_LOAD_INFO, read_stat_load, set_load_record);
}

