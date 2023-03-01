#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <coolbpf.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include "unity_nosched.h"
#include "sched_jit.h"
#include "unity_nosched.skel.h"
#include "../../../../unity/beeQ/beeQ.h"

#ifdef __x86_64__
#define	TIF_NEED_RESCHED	3
#elif defined (__aarch64__)
#define TIF_NEED_RESCHED	1
#endif

unsigned int nr_cpus;
struct sched_jit_summary summary, prev;

static void update_summary(struct sched_jit_summary* summary, const struct event *e)
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
	struct event *ev = (struct event *)data;

	e = *ev;
	e.delay = e.delay/(1000*1000);
	if (e.cpu > nr_cpus - 1)
		return;
	if (e.exit != 0)
		update_summary(&summary, &e);
}


DEFINE_SEKL_OBJECT(unity_nosched);

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
	int err, argfd, args_key;
	struct args args;

	bump_memlock_rlimit1();
	unity_nosched = unity_nosched_bpf__open();
	if (!unity_nosched) {
		err = errno;
		printf("failed to open BPF object\n");
		return -err;
	}

	err = unity_nosched_bpf__load(unity_nosched);
	if (err) {
		fprintf(stderr, "Failed to load BPF skeleton\n");
		DESTORY_SKEL_BOJECT(unity_nosched);
		return -err;
	}

	argfd = bpf_map__fd(unity_nosched->maps.args_map);
	args_key = 0;
	args.flag = TIF_NEED_RESCHED;
	args.thresh = 50*1000*1000;	/* 50ms */

	err = bpf_map_update_elem(argfd, &args_key, &args, 0);
	if (err) {
		fprintf(stderr, "Failed to update flag map\n");
		DESTORY_SKEL_BOJECT(unity_nosched);
		return err;
	}

	nr_cpus = libbpf_num_possible_cpus();
	memset(&summary, 0, sizeof(summary));
	{
		struct perf_thread_arguments *perf_args =
			malloc(sizeof(struct perf_thread_arguments));
		if (!perf_args) {
			printf("Failed to malloc perf_thread_arguments\n");
			DESTORY_SKEL_BOJECT(unity_nosched);
			return -ENOMEM;
		}
		memset(perf_args, 0, sizeof(struct perf_thread_arguments));
		perf_args->mapfd = bpf_map__fd(unity_nosched->maps.events);
		perf_args->sample_cb = handle_event;
		perf_args->lost_cb = handle_lost_events;
		perf_args->ctx = arg;
		perf_thread = beeQ_send_thread(arg, perf_args, thread_worker);
	}
	err = unity_nosched_bpf__attach(unity_nosched);
	if (err) {
		printf("failed to attach BPF programs: %s\n", strerror(err));
		DESTORY_SKEL_BOJECT(unity_nosched);
		return err;
	}
	
	printf("unity_nosched plugin install.\n");
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
	unity_set_index(line, 0, "mod", "noschd");
	unity_set_value(line, 0, "dltnum", delta(summary,num));
	unity_set_value(line, 1, "dlttm", delta(summary,total));
	unity_set_value(line, 2, "lt10ms", delta(summary,less10ms));
	unity_set_value(line, 3, "lt50ms", delta(summary,less50ms));
	unity_set_value(line, 4, "lt100ms", delta(summary,less100ms));
	unity_set_value(line, 5, "lt500ms", delta(summary,less500ms));
	unity_set_value(line, 6, "lt1s", delta(summary,less1s));
	unity_set_value(line, 7, "mts", delta(summary,plus1s));
	prev = summary;
	return 0;
}

void deinit(void)
{
	printf("unity_nosched plugin uninstall.\n");
	DESTORY_SKEL_BOJECT(unity_nosched);
}
