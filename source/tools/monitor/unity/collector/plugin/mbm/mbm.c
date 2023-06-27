#include <stdbool.h>
/*#define DEBUG	1*/
#include "mbm.h"
#include "cpuid.h"
#include "msr.h"
#include "public.h"

msr_t *msrs;
long nr_cpus;
cpuid_info cpuid_i;
__u64 summary[NR_EVENTS];
static int init_fail;
char *events_str[] = {"L3Occupancy", "MbmTotal", "MbmLocal"};

static bool is_resctrl_mounted(void)
{
	struct stat st;

	if (stat("/sys/fs/resctrl/mon_groups", &st) < 0) {
		return false;
	}
	return true;
}

static void discovery_resctrl_mon(void)
{
}

int init(void * arg)
{
	bool resctrl = false;
	int i, ret, cgroup_fd;

	ret = get_cpus(&nr_cpus);
	if (ret) {
		printf("WARN: pmu_events install FAIL sysconf\n");
		init_fail = ret;
		return 0;
	}

	resctrl = is_resctrl_mounted();
	/*if (resctrl)*/
	if (0) {
		discovery_resctrl_mon();
		goto end;
	}
	if (check_cpuid_support()) {
		cpuid_Factor_RMID_eventID(&cpuid_i);
		ret = init_msr(&msrs, cpuid_i.info.rmid-1);
		if (ret < 0) {
			printf("init_msr failed\n");
			return 0;
		}
	} else {
		printf(" not supported\n");
		return 1;
	}
end:
	printf("pmu_events plugin install.\n");
	init_fail = 0;
	return 0;
}

int collect(msr_t *msr, cpuid_info *cpu, __u64 *sum)
{
	int i;
	for (i = 0; i < nr_cpus; i++) {
		if (msr[i].fd <= 0)
			continue;
		else
			break;
	}
		if (cpu->info.eventid & 1<<L3CHE)
			sum[L3CHE] += extract_val(read_l3_cache(&msr[i]))*cpu->info.factor;
		if (cpu->info.eventid & 1<<MBTOL)
			sum[MBTOL] += extract_val(read_mb_total(&msr[i]))*cpu->info.factor;
		if (cpu->info.eventid & 1<<MBLOC)
			sum[MBLOC] += extract_val(read_mb_local(&msr[i]))*cpu->info.factor;
}

int fill_line(struct unity_line *line, __u64 *summ, char *mode, char *index)
{
	int i;

	unity_set_index(line, 0, mode, index);
	for (i = 0; i < NR_EVENTS; i++)
		unity_set_value(line, i, events_str[i], summ[i]);
}

int call(int t, struct unity_lines* lines)
{
	int i;
	__u64 sum[MAX_EVENT];
	struct unity_line* line;

	if (init_fail) {
		return init_fail;
	}

	collect(msrs, &cpuid_i, sum);

	unity_alloc_lines(lines, 1);
	line = unity_get_line(lines, 0);
	unity_set_table(line, "mbm");
	fill_line(line, sum, "mode", "msr");

	return 0;
}

void deinit(void)
{
	deinit_msr(msrs, nr_cpus);
	printf("pmu_events plugin uninstall\n");
}

#ifdef DEBUG
/* for dev/selftest */
int call_debug(__u64 *sum)
{
	int i;

	collect(msrs, &cpuid_i, sum);
}

char datas[1024*1024*1024];
int main(int argc, char *argv[])
{
	int ret, i = 8;
	__u64 sum[MAX_EVENT], tmp[MAX_EVENT];

	ret = init(NULL);
	if (ret)
		return ret;
	printf("eventid=%llu, factor=%llu\n",
		cpuid_i.info.eventid,
		cpuid_i.info.factor);
	memset(datas, 'a', sizeof(datas));
	call_debug(tmp);
	while(i > 0) {
		sleep(1);
		memset(sum, 0, sizeof(sum));
		memset(datas, 'b', sizeof(datas));
		call_debug(sum);
		printf("l3Oc=%lluKB, totalMB=%lluKB, localMB=%lluKB\n",
			sum[0]<tmp[0]?0:(sum[0]-tmp[0])/1024,
			sum[1]<tmp[1]?0:(sum[1]-tmp[1])/1024,
			sum[2]<tmp[2]?0:(sum[2]-tmp[2])/1024);
		tmp[0] = sum[0];
		tmp[1] = sum[1];
		tmp[2] = sum[2];
		i--;
	}
	printf("deinit\n");
	deinit();
}
#endif
