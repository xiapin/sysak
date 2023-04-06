#include "pmu_events.h"

struct pmu_events {
	int nr_cpus;
	struct pcpu_hw_info *pcpu_hwi;
};

long nr_cpus;
double summary[NR_EVENTS];
struct pcpu_hw_info *gpcpu_hwi;
struct pmu_events *glb_pme;
char *events_str[] = {"cycles", "ins", "refCyc",
			"llcLoad", "llcLoadMis",
			"llcStore", "llcStoreMis"};
char *value_str[] = {"cycles", "instructions", "CPI",
			"llc_load_ref", "llc_load_miss", "LLC_LMISS_RATE"
			"llc_store_ref", "llc_store_miss", "LLC_SMIRSS_RATE"};
/*char origpath[]="/mnt/host/sys/fs/cgroup/perf_event/system.slice/"; */
char *origpath = NULL;	/* defalt to host events */

static int init_fail;
static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
			   int cpu, int group_fd, unsigned long flags)
{
	int ret;

	ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
			group_fd, flags);	
	return ret;
}

int create_hw_events(struct pcpu_hw_info *pc_hwi)
{
	int cpu, i, j, group_last, idx_fail;
	int ret, pid, group_leader;
	struct hw_info *hwi, *leader;
	unsigned long flags = 0;

	hwi = pc_hwi->hwi;
	struct perf_event_attr attr = {
		.freq = 0,
		.disabled = 1,
		.sample_period = 1000*1000*1000,
	};

	cpu = pc_hwi->cpu;
	pid = pc_hwi->pid;
	flags = pc_hwi->flags;
	leader = NULL;
	group_leader = -1;
	j = 0;
	group_last = groupidx[0];
	for (i = 0; i < NR_EVENTS; i++) {
		/* The next PERF types */
		if (groupidx[i] != group_last) {
			group_leader = -1;
			group_last = groupidx[i];
		}
		attr.type = hw_types[i];
		attr.config = hw_configs[i];
#ifdef DEBUG
		if (cpu == 0)
			printf("cpu=%d, type=%d, conf=%llu, pid=%d, gld=%d, flags=%d\n",
				cpu, attr.type, attr.config, pid, group_leader, flags);
#endif
		hwi[i].fd = perf_event_open(&attr, pid, cpu, group_leader, flags);
		if (hwi[i].fd <= 0) {
			int ret = errno;
			if (ret == ENODEV) {
				printf("cpu may OFF LINE\n");
			} else {
				fprintf(stderr, "WARN:%s cpu%d event %s \n",
					strerror(ret), cpu, events_str[i]);
				break;
			}
		}
		hwi[i].leader = group_leader;
		/* group leader */
		if (group_leader == -1) {
			group_leader = hwi[i].fd;
		}
	}
	if (ret) {
		idx_fail = i;
		goto fail_open;
	}
	for (i = 0; i < NR_EVENTS; i++) {
		int ret = 0;
		if (hwi[i].leader == -1) { 
			ret = ioctl(hwi[i].fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
			if (ret < 0) {
				idx_fail = i;
				printf("FAIL:ioctl_RESET %s fd=%d fail\n", events_str[i], hwi[i].fd);
				goto fail_ioctl;
			}
			ret = ioctl(hwi[i].fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
			if (ret < 0) {
				idx_fail = i;
				printf("FAIL:ioctl_ENABLE %s fd=%d fail\n", events_str[i], hwi[i].fd);
				goto fail_ioctl;
			}
		}
	}
	return 0;
fail_ioctl:
	for (i = 0; i < idx_fail; i++) {
		if ((hwi[i].leader == -1) && (hwi[i].fd > 0))
			ioctl(hwi[i].fd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
		idx_fail = NR_EVENTS;
	}
fail_open:
	for (i = 0; i < idx_fail; i++) {
		if (hwi[i].fd > 0)
			close(hwi[i].fd);
	}
	return ret;
}

struct pmu_events *pme_new(int ncpu)
{
	int ret, fd;
	struct pmu_events *pmue;
	struct pcpu_hw_info *pcpu_hwi;

	pmue = calloc(sizeof(struct pmu_events), 1);
	if (!pmue) {
		ret = errno;
		fprintf(stderr, "%s :malloc pmu_events fail\n", strerror(ret));
		return NULL;
	}
	pcpu_hwi = calloc(sizeof(struct pcpu_hw_info), nr_cpus);
	if (!pcpu_hwi) {
		ret = errno;
		fprintf(stderr, "%s :alloc pcpu_hw_info fail\n", strerror(ret));
		free(pmue);
		return NULL;
	}
	pmue->nr_cpus = ncpu;
	pmue->pcpu_hwi = pcpu_hwi;
}

int init(void * arg)
{
	int i, ret, flags, cgroup_fd;
	struct pmu_events *pmue;
	struct pcpu_hw_info *pcpu_hwi;

	nr_cpus = sysconf(_SC_NPROCESSORS_CONF);
	if (nr_cpus <= 0) {
		ret = errno;
		printf("WARN: pmu_events install FAIL sysconf\n");
		init_fail = ret;
		return 0;
	}

	pmue = pme_new(nr_cpus);
	if (pmue && pmue->pcpu_hwi) {
		pcpu_hwi = pmue->pcpu_hwi;
		glb_pme = pmue;
	} else {
		init_fail = -1;
		return 0;
	}
#if 0
	pmue = (struct pmu_events *)arg;
	cgroup_fd = pmue->cgroup_fd;
#endif
	if (origpath) {
		cgroup_fd = open(origpath, O_RDONLY);
		if (cgroup_fd < 0) {
			printf(" open %s fail\n", origpath);
			init_fail = cgroup_fd;
			return 0;
		}
		flags = PERF_FLAG_PID_CGROUP;
	} else {
		cgroup_fd = -1;
		flags = 0;
	}
	for (i = 0; i < nr_cpus; i++) {
		pcpu_hwi[i].cpu = i;
		pcpu_hwi[i].pid = cgroup_fd;
		pcpu_hwi[i].flags = flags;
		ret = create_hw_events(&pcpu_hwi[i]);
		if (ret) {
			init_fail = ret;
			return 0;
		}
	}
	printf("pmu_events plugin install.\n");
	init_fail = 0;
	return 0;
}

void collect(struct pcpu_hw_info *phw, double *sum)
{
	int i, n;
	double *delta = phw->values;
	struct hw_info *hw;

	hw = phw->hwi;
	for (i = 0; i < NR_EVENTS; i++) {
		if (hw[i].fd < 0)
			continue;
		hw[i].prv_cnt = hw[i].count;
		n = read(hw[i].fd, &hw[i].count, sizeof(hw[i].count));
		if (n < 0)
			continue;
		delta[i] = hw[i].count - hw[i].prv_cnt;
		sum[i] += delta[i];
#ifdef DEBUG
		if (phw->cpu == 0)
		printf("cpu%d %s prev=%llu, now=%llu\n",
			phw->cpu, events_str[i],
			hw[i].prv_cnt, hw[i].count);
#endif
	}
#ifdef DEBUG
	printf("cpu%d cpi=%f\n", phw->cpu, (float)delta[0]/(float)delta[1]);
#endif
}

int fill_line(struct unity_line *line, double *summ, char *mode, char *index)
{
	int i;

	unity_set_index(line, 0, mode, index);
	for (i = 0; i < NR_EVENTS; i++)
		unity_set_value(line, i, events_str[i], summ[i]);

	unity_set_value(line, i++, "CPI",
		summ[INSTRUCTIONS]==0?0:summ[CYCLES]/summ[INSTRUCTIONS]);
	unity_set_value(line, i++, "IPC",
		summ[CYCLES]==0?0:summ[INSTRUCTIONS]/summ[CYCLES]);
	unity_set_value(line, i++, "MPI", 
		summ[INSTRUCTIONS]==0?0:
		(summ[LLC_LOAD_MISS]+summ[LLC_STORE_MISS])/summ[INSTRUCTIONS]);
	unity_set_value(line, i++, "l3LoadMisRate",
		summ[LLC_LOAD_REF]==0?0:summ[LLC_LOAD_MISS]/summ[LLC_LOAD_REF]);
	unity_set_value(line, i++, "l3StoreMisRate",
		summ[LLC_STORE_REF]==0?0:summ[LLC_STORE_MISS]/summ[LLC_STORE_REF]);
	unity_set_value(line, i++, "l3MisRate",
		(summ[LLC_LOAD_REF]+summ[LLC_STORE_REF])==0?0:
		(summ[LLC_LOAD_MISS]+summ[LLC_STORE_MISS])/
		(summ[LLC_LOAD_REF]+summ[LLC_STORE_REF]));
}

int call(int t, struct unity_lines* lines)
{
	int i;
	char index[16] = {0};
	struct unity_line* line;
	double summ[NR_EVENTS];
	struct pcpu_hw_info *pcp_hw;

	if (init_fail) {
		return init_fail;
	}
	pcp_hw = glb_pme->pcpu_hwi;
	for (i = 0; i < nr_cpus; i++) {
		collect(&pcp_hw[i], summ);
	}
	unity_alloc_lines(lines, 1+nr_cpus);
	line = unity_get_line(lines, 0);
	unity_set_table(line, "pmu_events");
	fill_line(line, summ, "mode", "node");

	for (i = 0; i < nr_cpus; i++) {
		line = unity_get_line(lines, 1+i);
		unity_set_table(line, "pmu_events_percpu");
		snprintf(index, sizeof(index), "%d", i);
		fill_line(line, pcp_hw[i].values, "core", index);
	}
	return 0;
}

void remove_events(struct pcpu_hw_info *pcpu_hwi)
{

	int i;
	struct hw_info *hw;

	hw = pcpu_hwi->hwi;
	for (i = 0; i < NR_EVENTS; i++) {
		if ((hw[i].leader == -1) && (hw[i].fd > 0))
			ioctl(hw[i].fd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
		if (hw[i].fd > 0) {
			close(hw[i].fd);
		}
	}
}

void deinit(void)
{
	int i;
	struct pcpu_hw_info *pcp_hw;

	if (glb_pme) {
		pcp_hw = glb_pme->pcpu_hwi;
		if (pcp_hw) {
			for (i = 0; i < nr_cpus; i++) {
				remove_events(&pcp_hw[i]);
			}
			free(pcp_hw);
		}
		free(glb_pme);
	}
	printf("pmu_events plugin uninstall\n");
}

#ifdef DEBUG
/* for dev/selftest */
int call_debug(void)
{
	int i;
	struct pcpu_hw_info *pcp_hw;

	pcp_hw = glb_pme->pcpu_hwi;
	for (i = 0; i < nr_cpus; i++) {
		collect(&pcp_hw[i], summary);
	}
}
int main(int argc, char *argv[])
{
	int i = 4;

	init(NULL);
	call_debug();
	while(i > 0) {
		sleep(1);
		memset(summary, 0, sizeof(summary));
		call_debug();
		if (summary[INSTRUCTIONS])
			printf("CPI=%f\n", summary[CYCLES]/summary[INSTRUCTIONS]);
		if (summary[INSTRUCTIONS])
			printf("RCPI=%f\n", summary[REF_CYCLES]/summary[INSTRUCTIONS]);
		if (summary[LLC_LOAD_REF])
			printf("LLC_LOAD_MISS RATE =%f\n", summary[LLC_LOAD_MISS]/summary[LLC_LOAD_REF]);
		if (summary[LLC_STORE_REF])
			printf("LLC_STORE_MISS RATE =%f\n", summary[LLC_STORE_MISS]/summary[LLC_STORE_REF]);
		if ((summary[LLC_LOAD_REF]+summary[LLC_STORE_REF]) != 0)
			printf("LLC_MISS RATE=%f\n",
				(summary[LLC_LOAD_MISS]+summary[LLC_STORE_MISS])/
				(summary[LLC_LOAD_REF]+summary[LLC_STORE_REF]));
		i--;
	}
	deinit();
}
#endif
