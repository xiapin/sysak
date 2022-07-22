#include <argp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include "schedmoni.h"
#include "./bpf/schedmoni.skel.h"

extern struct env env;
extern int nr_cpus;
extern FILE *fp_irq;
extern volatile sig_atomic_t exiting;

static int stack_fd;
struct ksym *ksyms;

static int
open_and_attach_perf_event(struct perf_event_attr *attr,
			   struct bpf_program *prog,
			   struct bpf_link *links[])
{
	int i, fd;

	for (i = 0; i < nr_cpus; i++) {
		fd = syscall(__NR_perf_event_open, attr, -1, i, -1, 0);
		if (fd < 0) {
			/* Ignore CPU that is offline */
			if (errno == ENODEV)
				continue;
			fprintf(stdout, "failed to init perf sampling: %s\n",
				strerror(errno));
			return -1;
		}
		links[i] = bpf_program__attach_perf_event(prog, fd);
		if (!links[i]) {
			fprintf(stdout, "failed to attach perf event on cpu: %d\n", i);
			close(fd);
			return -1;
		}
	}
	return 0;
}

int attach_prog_to_perf(struct schedmoni_bpf *obj, struct bpf_link **sw_mlinks, struct bpf_link **hw_mlinks)
{
	int ret = 0;

	struct perf_event_attr attr_hw = {
		.type = PERF_TYPE_HARDWARE,
		.freq = 0,
		.sample_period = env.sample_period*2,	/* refer to watchdog_update_hrtimer_threshold() */
		.config = PERF_COUNT_HW_CPU_CYCLES,
	};

	struct perf_event_attr attr_sw = {
		.type = PERF_TYPE_SOFTWARE,
		.freq = 0,
		.sample_period = env.sample_period,
		.config = PERF_COUNT_SW_CPU_CLOCK,
	};

	if (!open_and_attach_perf_event(&attr_hw, obj->progs.hw_irqoff_event, hw_mlinks)) {
		ret = 1<<PERF_TYPE_SOFTWARE;
		if (!open_and_attach_perf_event(&attr_sw, obj->progs.sw_irqoff_event1, sw_mlinks))
			ret = ret | 1<<PERF_TYPE_SOFTWARE;

	} else {
		if (!open_and_attach_perf_event(&attr_sw, obj->progs.sw_irqoff_event2, sw_mlinks))
			ret = 1<<PERF_TYPE_SOFTWARE;
	}
	return !ret;
}

static char stack[512];
void irqoff_handle_event(void *ctx, int cpu, void *data, __u32 data_sz)
{
	struct event ev, *e;
	const struct event *ep = data;
	char ts[64];

	ev = *ep;
	e = &ev;
	stamp_to_date(e->stamp, ts, sizeof(ts));
	e->delay = e->delay/(1000*1000);
	if (env.mod_json) {
		int cnt;
		struct summary *sum;
		cJSON *arryItem;
		char current[32] = {0};

		sum = &env.summary[IRQOF];
		arryItem = cJSON_CreateObject();
		cJSON_AddStringToObject(arryItem, "date", ts);
		cJSON_AddStringToObject(arryItem, "class", "irq延迟");
		cJSON_AddNumberToObject(arryItem, "latency", e->delay);
		cJSON_AddNumberToObject(arryItem, "cpu", e->cpuid);
		snprintf(current, sizeof(current), "%s (%d)", e->task, e->pid);
		cJSON_AddStringToObject(arryItem, "current", current);
		cJSON_AddNumberToObject(arryItem, "stamp", e->stamp);

		cnt = print_stack(stack_fd, e->ret, 7, ksyms, stack, MOD_STRING);
		*(stack+cnt) = '\0';
		cJSON_AddStringToObject(arryItem, "extern", stack);
		cJSON_AddItemToArray(env.json.tbl_data, arryItem);
		sum->delay += e->delay;
		sum->cnt++;
		sum->real_cnt++;
		if (e->delay > sum->max)
			sum->max = e->delay;
	} else {
		fprintf(fp_irq, "%-21s %-5d %-15s %-8d %-10llu\n",
			ts, e->cpuid, e->task, e->pid, e->delay);
		print_stack(stack_fd, e->ret, 0, ksyms, fp_irq, MOD_FILE);
	}
}

void irqoff_handler(void *args)
{
	struct tharg *runirq = (struct tharg *)args;
	int err = 0, poll_fd;
	struct perf_buffer *pb = NULL;
	struct perf_buffer_opts pb_opts = {};

	stack_fd = runirq->ext_fd;
	poll_fd = runirq->map_fd;
	fprintf(fp_irq, "%-21s %-5s %-15s %-8s %-10s\n",
		"TIME(irqoff)", "CPU", "COMM", "TID", "LAT(us)");

	pb_opts.sample_cb = irqoff_handle_event;
	pb = perf_buffer__new(poll_fd, 64, &pb_opts);
	if (!pb) {
		err = -errno;
		fprintf(stdout, "failed to open perf buffer: %d\n", err);
		goto clean_irqoff;
	}

	while (!exiting) {
		err = perf_buffer__poll(pb, 100);
		if (err < 0 && err != -EINTR) {
			fprintf(stdout, "error polling perf buffer: %s\n", strerror(-err));
			goto clean_irqoff;
		}
		/* reset err to return 0 if exiting */
		err = 0;
	}

clean_irqoff:
	perf_buffer__free(pb);
}

