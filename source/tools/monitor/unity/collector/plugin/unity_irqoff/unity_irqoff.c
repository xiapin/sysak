#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
//#include <sys/stat.h>
//#include <fcntl.h>
//#include <sys/mman.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <coolbpf.h>
#include <limits.h>
#include "unity_irqoff.h"
#include "sched_jit.h"
#include "unity_irqoff.skel.h"
#include "../../../../unity/beeQ/beeQ.h"

struct env {
	__u64 sample_period;
	__u64 threshold;
} env = {
	.threshold = 50*1000*1000,	/* 10ms */
};

static int nr_cpus;
struct sched_jit_summary summary, prev;
struct bpf_link **sw_mlinks, **hw_mlinks= NULL;

DEFINE_SEKL_OBJECT(unity_irqoff);

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
			fprintf(stderr, "failed to init perf sampling: %s\n",
				strerror(errno));
			return -1;
		}
		links[i] = bpf_program__attach_perf_event(prog, fd);
		if (!links[i]) {
			fprintf(stderr, "failed to attach perf event on cpu: %d\n", i);
			close(fd);
			return -1;
		}
	}
	return 0;
}

/* surprise! return 0 if failed! */
static int attach_prog_to_perf(struct unity_irqoff_bpf *obj)
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
	return ret;
}

static void
update_summary(struct sched_jit_summary* summary, const struct event *e)
{
	summary->num++;
	summary->total += e->delay;

	if (e->delay < 10) {
		summary->less10ms++;
	} else if (e->delay < 50) {
		summary->less50ms++;
	} else if (e->delay < 100) {
		summary->less100ms++;
	} else if (e->delay < 500) {
		summary->less500ms++;
	} else if (e->delay < 1000) {
		summary->less1s++;
 	} else {
		summary->plus1s++;
	}
}

void handle_event(void *ctx, int cpu, void *data, __u32 data_sz)
{
	struct event e;
	e = *((struct event *)data);
	e.delay = e.delay/(1000*1000);
	if (e.cpu > nr_cpus - 1)
		return;
	update_summary(&summary, &e);
}

static int irqoff_handler(void *arg, struct unity_irqoff_bpf *unity_irqoff)
{
	int arg_key = 0, err = 0;
	struct arg_info arg_info = {};
	int arg_fd;

	/*memset(summary, 0, sizeof(struct sched_jit_summary)); */
	struct perf_thread_arguments *perf_args =
		calloc(sizeof(struct perf_thread_arguments), 1);
	if (!perf_args) {
		printf("Failed to malloc perf_thread_arguments\n");
		DESTORY_SKEL_BOJECT(unity_irqoff);
		return -ENOMEM;
	}
	perf_args->mapfd = bpf_map__fd(unity_irqoff->maps.events);
	perf_args->sample_cb = handle_event;
	perf_args->lost_cb = handle_lost_events;
	perf_args->ctx = arg;
	perf_thread = beeQ_send_thread(arg, perf_args, thread_worker);

	arg_fd = bpf_map__fd(unity_irqoff->maps.arg_map);
	arg_info.thresh = env.threshold;
	env.sample_period = env.threshold*2/5;
	err = bpf_map_update_elem(arg_fd, &arg_key, &arg_info, 0);
	if (err) {
		fprintf(stderr, "Failed to update arg_map\n");
		return err;
	}

	if (!(err = attach_prog_to_perf(unity_irqoff)))
		return err;
	return 0;
}

static void bump_memlock_rlimit1(void)
{
	struct rlimit rlim_new = {
		.rlim_cur	= RLIM_INFINITY,
		.rlim_max	= RLIM_INFINITY,
	};

	if (setrlimit(RLIMIT_MEMLOCK, &rlim_new)) {
		fprintf(stderr, "Failed to increase RLIMIT_MEMLOCK limit!\n");
		exit(1);
	}
}

int init(void *arg)
{
	int err;

	nr_cpus = libbpf_num_possible_cpus();
	if (nr_cpus < 0) {
		fprintf(stderr, "failed to get # of possible cpus: '%s'!\n",
			strerror(-nr_cpus));
		return nr_cpus;
	}

	bump_memlock_rlimit1();

	sw_mlinks = calloc(nr_cpus, sizeof(*sw_mlinks));
	if (!sw_mlinks) {
		err = errno;
		fprintf(stderr, "failed to alloc sw_mlinks or rlinks\n");
		return err;
	}

	hw_mlinks = calloc(nr_cpus, sizeof(*hw_mlinks));
	if (!hw_mlinks) {
		err = errno;
		fprintf(stderr, "failed to alloc hw_mlinks or rlinks\n");
		free(sw_mlinks);
		return err;
	}

	unity_irqoff = unity_irqoff_bpf__open_and_load();
	if (!unity_irqoff) {
		err = errno;
		fprintf(stderr, "failed to open and/or load BPF object\n");
		return err;
	}

	irqoff_handler(arg, unity_irqoff);

	return 0;
}
#define delta(sum, value)	\
	sum.value - prev.value
int call(int t, struct unity_lines *lines)
{
	struct unity_line *line;

	unity_alloc_lines(lines, 1);
	line = unity_get_line(lines, 0);
	unity_set_table(line, "sched_moni_jitter");
	unity_set_index(line, 0, "mod", "irqoff");
	unity_set_value(line, 0, "dltnum", delta(summary, num));
	unity_set_value(line, 1, "dlttm", delta(summary, total));
	unity_set_value(line, 2, "gt50ms", delta(summary, less100ms));
	unity_set_value(line, 3, "gt100ms", delta(summary, less500ms));
	unity_set_value(line, 4, "gt500ms", delta(summary, less1s));
	unity_set_value(line, 5, "gt1s", delta(summary, plus1s));
	prev = summary;
	return 0;
}

void deinit(void)
{
	free(sw_mlinks);
	free(hw_mlinks);
	printf("unity_irqoff plugin uninstall.\n");
	DESTORY_SKEL_BOJECT(unity_irqoff);
}
