#include "pmu_cpi.h"

long nr_cpus;
__u64 summary[NR_EVENTS];
struct pcpu_hw_info *pcpu_hwi;
char *events_str[] = {"cpu_cycles", "instructions"};

struct hw_info def_hwi[NR_EVENTS] = {
	{
		.type = PERF_TYPE_HARDWARE,
		.config = PERF_COUNT_HW_CPU_CYCLES,
	},
	{
		.type = PERF_TYPE_HARDWARE,
		.config = PERF_COUNT_HW_INSTRUCTIONS,
	},
#if 0
	{
		.type = PERF_TYPE_HW_CACHE;
		.config = 
	},
	{
		.type = PERF_TYPE_HW_CACHE;
		.config = 
	},
#endif
};

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
	struct hw_info *hwi;
	int cpu;
	int i, pid, group_leader = -1;
	unsigned long flags = 0;
	unsigned long long hw_events_cnt[PERF_COUNT_HW_MAX];

	hwi = pc_hwi->hw;
	struct perf_event_attr attr = {
		.freq = 0,
		.disabled = 1,
		.sample_period = 1000*1000*1000,
	};

	cpu = pc_hwi->cpu;
	pid = pc_hwi->pid;
	flags = pc_hwi->flags;
	for (i = 0; i < NR_EVENTS; i++) {
		hwi[i] = def_hwi[i];
		attr.type = hwi[i].type;
		attr.config = hwi[i].config;
#ifdef DEBUG
		printf("cpu=%d, type=%d, conf=%llu, pid=%d, gld=%d, flags=%d\n",
			cpu, attr.type, attr.config, pid, group_leader, flags);
#endif
		hwi[i].fd = perf_event_open(&attr, pid, cpu, group_leader, flags);
		if (hwi[i].fd < 0) {
			int ret = errno;
			fprintf(stderr, "WARN:%s cpu%d event %s \n", strerror(ret), cpu, events_str[i]);
			if (ret == ENODEV)
				printf("cpu may OFF LINE\n");
		}
		/* group leader */
		if (i == 0)
			group_leader = hwi[i].fd;
	}
	ioctl(group_leader, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
	ioctl(group_leader, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
	pc_hwi->fd_leader = group_leader;
}

int init(void * arg)
{
	int i, ret;

	nr_cpus = sysconf(_SC_NPROCESSORS_CONF);
	if (nr_cpus <= 0) {
		ret = errno;
		printf("WARN: pmu_cpi install FAIL sysconf\n");
		return -ret;
	}

	pcpu_hwi = calloc(sizeof(struct pcpu_hw_info), nr_cpus);
	if (!pcpu_hwi) {
		ret = errno;
		fprintf(stderr, "%s :malloc hw_info fail\n", strerror(ret));
		return -ret;
	}
	for (i = 0; i < nr_cpus; i++) {
		pcpu_hwi[i].cpu = i;
		pcpu_hwi[i].pid = -1;
		pcpu_hwi[i].flags = 0;
		create_hw_events(&pcpu_hwi[i]);
	}

	printf("pmu_cpi plugin install.\n");
	return 0;
}

void collect(struct pcpu_hw_info *phw, __u64 *sum)
{
	__u64 delta[2];
	int cgrp_fd, i, *fd;
	struct hw_info *hw;
	unsigned long long *hw_events_cnt;

	hw = phw->hw;
	for (i = 0; i < NR_EVENTS; i++) {
		if (hw[i].fd < 0)
			continue;
		hw[i].prv_cnt = hw[i].count;
		read(hw[i].fd, &hw[i].count, sizeof(hw[i].count));
		delta[i] = hw[i].count - hw[i].prv_cnt;
		sum[i] += delta[i];
#ifdef DEBUG
		printf("cpu%d %s:%llu\n", phw->cpu, events_str[i], delta[i]);
#endif
	}
#ifdef DEBUG
	printf("cpu%d cpi=%f\n", phw->cpu, (float)delta[0]/(float)delta[1]);
#endif
}

int fill_line(struct unity_line *line)
{
	double cycles, instructions;
	cycles = summary[0];
	instructions = summary[1];

	unity_set_value(line, 0, "cycles", cycles);
	unity_set_value(line, 1, "instructions", instructions);
	unity_set_value(line, 2, "ipc", instructions/cycles);
}

int call(int t, struct unity_lines* lines) {
	struct unity_line* line;

	unity_alloc_lines(lines, 1);
	line = unity_get_line(lines, 0);
	unity_set_table(line, "pmu_cpi");
	fill_line(line);

	return 0;
}

void remove_events(struct pcpu_hw_info *pcpu_hwi)
{
	int i;

	ioctl(pcpu_hwi->fd_leader, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
	for (i = 0; i < NR_EVENTS; i++) {
		struct hw_info *hw;

		hw = pcpu_hwi->hw;
		if (hw[i].fd > 0) {
			close(hw[i].fd);
		}
	}
}

void deinit(void)
{
	int i;

	for (i = 0; i < nr_cpus; i++) {
		remove_events(&pcpu_hwi[i]);
	}
	if (pcpu_hwi)
		free(pcpu_hwi);

	printf("pmu_cpi plugin uninstall\n");
}

#ifdef DEBUG
int call(void)
{
	int i;

	for (i = 0; i < nr_cpus; i++) {
		collect(&pcpu_hwi[i], summary);
	}
}
int main(int argc, char *argv[])
{
	int i = 4;

	init(NULL);
	call();
	while(i > 0) {
		sleep(1);
		summary[0] = summary[1] = 0;
		call();
		printf("sum cpi=%f\n", (float)summary[0]/(float)summary[1]);
		i--;
	}
	deinit();
}
#endif
