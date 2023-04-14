#include "cg_pmu_ffi.h"

void my_sleep(int x)
{
	sleep(x);
	printf("sleep end\n");
}

char *events_str[] = {"cpu_cycles", "instructions", "ref_cycles",
			"llc_load_ref", "llc_load_miss",
			"llc_store_ref", "llc_store_miss"};

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
			   int cpu, int group_fd, unsigned long flags)
{
	int ret;

	ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
			group_fd, flags);	
	return ret;
}

static int create_pcpu_hw_events(struct pcpu_hwi *hwi, struct dyn_arg* arg)
{
	int i, j, fd, group;
	struct event_info *ei;
	struct perf_event_attr attr = {
		.freq = 0,
		.disabled = 1,
		.sample_period = 1000*1000*1000,
	};

	group = -1;
	ei = hwi->ei;
	for (i = 0; i < NR_EVENTS; i++) {
		attr.type = static_args[i].type;
		attr.config = static_args[i].config;
		fd = perf_event_open(&attr, arg->pid, arg->cpu, group, arg->flags);
		if (fd > 0) {
			ioctl(fd, PERF_EVENT_IOC_RESET, 0);
			ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
			ei[i].fd = fd;
#ifdef DEBUG
			printf("cpu%d %s fd=%d\n", arg->cpu, events_str[i], fd);
#endif
		} else {
			printf("cpu%d perf_event_open fail\n", arg->cpu);
			goto create_fail;
		}
	}
	return 0;

create_fail:
	for (j = 0; j < i; j++) {
		if (ei[j].fd > 0) {
			ioctl(ei[j].fd, PERF_EVENT_IOC_DISABLE, 0);
			close(ei[j].fd);
		}
	}
	return -1;
}

int create_hw_events(struct pcpu_hwi *hwi, int nr_cpus, char *origpath)
{
	int i, cgfd;
	struct dyn_arg args;
#if 0
	printf("create_hw_events \n");
	return 0;
#endif
	cgfd = open(origpath, O_RDONLY);
	if (cgfd > 0) {
		args.flags = PERF_FLAG_PID_CGROUP;
		args.pid = cgfd;
	} else {
		return -errno;
	}
	for (i = 0; i < nr_cpus; i++) {
		args.cpu = i;
		create_pcpu_hw_events(&hwi[i], &args);
	}
}

static int collect_pcpu_events(struct pcpu_hwi *hwi, double *sum)
{
	int n, enumo;
	struct event_info *ei;
	int i, fd;

	n = 0;
	ei = hwi->ei;
	for (i = 0; i < NR_EVENTS; i++) {
		fd = ei[i].fd;
		if (fd > 0) {
			n = read(fd, &ei[i].cnt, sizeof(__u64));
			if (n < 0) {
				enumo = errno;
#ifdef DEBUG
				printf(" read fd%d fail:%s \n", fd, strerror(enumo));
#endif
				return -enumo;
			}
			ei[i].delta = ei[i].cnt - ei[i].prev;
			ei[i].prev = ei[i].cnt;
			sum[i] = sum[i] + ei[i].delta;
		} else {
//#ifdef DEBUG
			printf("collect_pcpu_events: fd = %d\n", fd);
//#endif
		}
	}
	return n;
}

int collect_events(struct pcpu_hwi *hwi, int nr_cpus, double *sum)
{
	int i, n;

	for (i = 0; i < nr_cpus; i++) {
		n = collect_pcpu_events(&hwi[i], sum);
		if (n < 0) {
			printf("collect_pcpu_events cpu%d fail:%s\n", i, strerror(-n));
			break;
		}
	}
	if (n < 0)
		return n;
	return 0;
}

int stop_events(struct pcpu_hwi *hwi, int nr_cpus)
{
	int i, j, fd;
	struct event_info *ei;
	unsigned long long sum[NR_EVENTS] = {0};

	for(i = 0; i < nr_cpus; i++) {
		ei = hwi[i].ei;
		for (j = 0; j < NR_EVENTS; j++) {
			fd = ei[j].fd;
			if (fd > 0) {
#ifdef DEBUG
				printf("cpu%d %s delta=%llu\n", i, events_str[j], ei[j].delta);
#endif
				sum[j] = sum[j] + ei[j].delta;
				ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
				close(fd);
			} else {
#ifdef DEBUG
				printf("Fail: cpu%d %s fd=%d\n", i, events_str[j], fd);
#endif
			}
		}
	}
	return 0;
}
