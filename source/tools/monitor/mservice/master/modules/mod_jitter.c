#include "tsar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <linux/types.h>
#include <linux/major.h>
#include "sched_jit.h"

#define NR_MODS	3
#define BUF_SIZE	(NR_MODS * 4096)

char g_buf[BUF_SIZE];
char line[512];
int jitter_init = 0;
char *jitter_usage = "    --jit                Application jitter stats";
char *jit_mod[] = {"rqslow", "noschd", "irqoff"};
char *jit_shm_key = "sysak_mservice_jitter_shm";

struct exten_sum {
	long delta_num;
	U_64 delta_total;
};

struct exten_sum_idx {
	int idx;
	struct exten_sum sum[NR_MODS];
};

struct jitter_shm {
	struct sched_jit_summary nosched;
	struct sched_jit_summary irqoff;
	struct sched_jit_summary rqslow;
};

static struct jitter_shm *jitshm = NULL;
struct exten_sum_idx sum_ex;
static struct mod_info jitter_info[] = {
	/* 0~9 */
	{"   num", DETAIL_BIT,  0,  STATS_NULL},	/* total numbers of happend */
	{"  time", DETAIL_BIT,  0,  STATS_NULL},	/* the sum-time of delay */
	{"dltnum", SUMMARY_BIT,  0,  STATS_NULL},	/* delta numbers of happend */
	{" dlttm", SUMMARY_BIT,  0,  STATS_NULL},	/* the delta time of delay */
	{" <10ms", DETAIL_BIT,  0,  STATS_NULL},	/* less than 10ms */
	{" <50ms", DETAIL_BIT,  0,  STATS_NULL},	/* less than 50ms */
	{"<100ms", DETAIL_BIT,  0,  STATS_NULL},	/* less than 100ms */
	{"<500ms", DETAIL_BIT,  0,  STATS_NULL},	/* less than 500ms */
	{"   <1s", DETAIL_BIT,  0,  STATS_NULL},	/* less than 1s */
	{"   >1s", DETAIL_BIT,  0,  STATS_NULL},	/* more than 1s */
};

#define NR_JITTER_INFO sizeof(jitter_info)/sizeof(struct mod_info)

static int cg_jitter_inited(void)
{
	int *cg_jitter_symbol;
	int jit_inited;
	void *handle = dlopen(NULL, RTLD_LAZY);
	if (handle) {
		cg_jitter_symbol = dlsym(handle, "cg_jitter_init");
		if (cg_jitter_symbol)
			jit_inited = *cg_jitter_symbol + jitter_init;
		else
			jit_inited = jitter_init;
	} else {
		fprintf(stderr, "jitter:dlopen NULL fail\n");
		jit_inited = -1;
	}
	return jit_inited;
}

static int check_and_init_shm(void)
{
	int fd, ret;
	char *p;
	static int jitshm_inited = 0;

	if (jitshm_inited) {
		return 0;
	}

	fd = shm_open(jit_shm_key, O_CREAT|O_RDWR|O_TRUNC, 06666);
	if (fd < 0) {
		ret = errno;
		perror("shm_open jit_shm_key");
		return ret;
	}

	ret = ftruncate(fd, sizeof(struct jitter_shm));
	if (ret < 0) {
		ret = errno;
		perror("ftruncate jit_shm_key");
		goto shm_fail;
	}

	p = mmap(NULL, sizeof(struct jitter_shm),
		PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (p == MAP_FAILED) {
		ret = errno;
		perror("mmap jit_shm_key");
		goto shm_fail;
	}

	jitshm_inited = 1;
	jitshm = (struct jitter_shm *)p;

	return 0;

shm_fail:
	shm_unlink(jit_shm_key);
	return ret;
}

int init_jitter(void)
{
	int ret;
	FILE *fp1, *fp2, *fp3;

	ret = check_and_init_shm();
	if (ret < 0)
		return ret;

	ret = cg_jitter_inited();
	if (ret > 0)
		return 0;
	else if (ret < 0)
		return ret;

	/* todo: what if command can't be find? */
	/* threshold is 40ms */
	fp1 = popen("sysak runqslower -S sysak_mservice_jitter_shm 40 2>/dev/null &", "r");
	if (!fp1) {
		perror("popen runqslower");
		return -1;
	}

	fp2 = popen("sysak nosched -S sysak_mservice_jitter_shm -t 10 2>/dev/null &", "r");
	if (!fp2) {
		perror("popen nosched");
		return -1;
	}

	fp3 = popen("sysak irqoff -S sysak_mservice_jitter_shm  10 2>/dev/null &", "r");
	if (!fp3) {
		perror("popen irqoff");
		return -1;
	}
	jitter_init = 1;

	return 0;
}

void print_jitter_stats(struct module *mod)
{
	int i, pos;
	struct sched_jit_summary *jitsum[NR_MODS];
	memset(g_buf, 0, BUF_SIZE);

	pos = 0;
	jitsum[0] = &jitshm->nosched;
	jitsum[1] = &jitshm->irqoff;
	jitsum[2] = &jitshm->rqslow;
	for (i = 0; i < NR_MODS; i++) {
		pos += snprintf(g_buf + pos, BUF_SIZE - pos,
			"%s=%ld,%llu,%ld,%llu,%ld,%ld,%ld,%ld,%ld,%ld, %d" ITEM_SPLIT,
			jit_mod[i], jitsum[i]->num, jitsum[i]->total,
			sum_ex.sum[i].delta_num, sum_ex.sum[i].delta_total,
			jitsum[i]->less10ms, jitsum[i]->less50ms,
			jitsum[i]->less100ms, jitsum[i]->less500ms,
			jitsum[i]->less1s, jitsum[i]->plus1s,pos);
	}
	set_mod_record(mod, g_buf);
}

void read_jitter_stat(struct module *mod, char *parameter)
{
	int ret;
	ret = init_jitter();
	if (ret)
		fprintf(stderr, "init_jitter failed\n");/*todo*/

	print_jitter_stats(mod);
}

static void
set_jitter_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
	int idx;

	st_array[0] = cur_array[0];
	st_array[1] = cur_array[1];
	st_array[4] = cur_array[4];
	st_array[5] = cur_array[5];
	st_array[6] = cur_array[6];
	st_array[7] = cur_array[7];
	st_array[8] = cur_array[8];
	st_array[9] = cur_array[9];

	if (cur_array[0] >= pre_array[0])
		st_array[2] = cur_array[0] - pre_array[0];
	else
		st_array[2] = -1;
	if (cur_array[1] >= pre_array[1])
		st_array[3] = cur_array[1] - pre_array[1];
	else
		st_array[3] = -1;

	idx = sum_ex.idx;
	sum_ex.sum[idx].delta_num = st_array[2];
	sum_ex.sum[idx].delta_total = st_array[3];
	sum_ex.idx = (idx+1)%(mod->n_item);
	//pre_array[0] = st_array[0];
	//pre_array[1] = st_array[1];
}

void
mod_register(struct module *mod)
{
	register_mod_fields(mod, "--jit", jitter_usage, jitter_info,
			NR_JITTER_INFO, read_jitter_stat, set_jitter_record);
}
