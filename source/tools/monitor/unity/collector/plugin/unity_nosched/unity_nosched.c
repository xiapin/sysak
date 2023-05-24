#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <coolbpf.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "unity_nosched.h"
#include "sched_jit.h"
#include "unity_nosched.skel.h"
#include "../../../../unity/beeQ/beeQ.h"

#define MAX_NOSCHED_ELEM	1024
#ifdef __x86_64__
#define	TIF_NEED_RESCHED	3
#elif defined (__aarch64__)
#define TIF_NEED_RESCHED	1
#endif

char log_dir[] = "/var/log/sysak/nosched";
char filename[] = "/var/log/sysak/nosched/unity_nosched.log";

struct evelem {
	struct event e;
	time_t t;
};

struct stackinfo {
	int stackfd, fhead, bhead;
	struct evelem elem[MAX_NOSCHED_ELEM];
};

FILE *logfp;
unsigned int nr_cpus;
struct sched_jit_summary summary, prev;
struct stackinfo globEv;

static struct ksym *ksyms;
void print_stack(int fd, __u32 ret, struct ksym *syms, FILE *fp);
int load_kallsyms(struct ksym **pksyms);

static int prepare_directory(char *path)
{
	int ret;

	ret = mkdir(path, 0777);
	if (ret < 0 && errno != EEXIST)
		return errno;
	else
		return 0;
}

void flush_to_file(struct stackinfo *ev, int index)
{
	int tmphd;

	if (ev->fhead == ev->bhead)
		return;
	if (index < 0)
		index = ev->bhead;
	printf("will flush: fhead=%d, bhead=%d, index=%d\n",
		ev->fhead, ev->bhead, index);
	tmphd = ev->fhead;
	ev->fhead = index;
	if (index < tmphd)
		index = index + MAX_NOSCHED_ELEM;
	for (; tmphd < index; tmphd++) {
		char ts[64];
		struct tm *tm;
		int i = tmphd%MAX_NOSCHED_ELEM;
		tm = localtime(&globEv.elem[i].t);
		strftime(ts, sizeof(ts), "%F %H:%M:%S", tm);
		fprintf(logfp, "%-18.6f %-5d %-15s %-8d %-10llu %-21s\n",
			((double)globEv.elem[i].e.stamp/1000000000), globEv.elem[i].e.cpu,
			globEv.elem[i].e.comm, globEv.elem[i].e.pid, globEv.elem[i].e.delay,
			(globEv.elem[i].e.exit==globEv.elem[i].e.stamp)?"(EOF)":ts);
		print_stack(globEv.stackfd, globEv.elem[i].e.ret, ksyms, logfp);
		fflush(logfp);
	}
}

static void update_summary(struct sched_jit_summary* summary, const struct event *e)
{
	summary->num++;
	summary->total += e->delay;
	if (e->delay < 10) {
		summary->less10ms++;
	} else if (e->delay < 50) {
		summary->less50ms++;
	} else if (e->delay < 100) {
		summary->less100ms++;	/* gt50 */
	} else if (e->delay < 500) {
		summary->less500ms++;	/* gt100 */
	} else if (e->delay < 1000) {
		summary->less1s++;	/* gt500 */
	} else {
		summary->plus1s++;	/* gt1s */
	}
}

void record_stack(struct event *e)
{
	time_t t;
	int idex = globEv.bhead;

	time(&t);
	globEv.elem[idex].e = *e;
	globEv.elem[idex].t = t;
	idex++;
	if (idex > MAX_NOSCHED_ELEM) {
		/*flush_to_file(&globEv, 0);*/
		globEv.bhead = 0;
	} else {
		globEv.bhead = idex;
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
	if (e.delay > 100) {	/* we use 100ms to control the frequency of output */
		printf("%s[%d] delay=%lld\n", e.comm, e.pid, e.delay);
		record_stack(&e);
	}
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
	struct args args;
	int err, ret, argfd, args_key;

	err = prepare_directory(log_dir);
	if (err) {
		printf("prepare_dictory fail\n");
		return err;
	}
	logfp = fopen(filename, "w+");
	if (!logfp) {
		int ret = errno;
		fprintf(stderr, "%s :fopen %s\n",
		strerror(errno), filename);
		return ret;
	}
	bump_memlock_rlimit1();
	ksyms = NULL;
	err = load_kallsyms(&ksyms);
	if (err) {
		fprintf(stderr, "Failed to load kallsyms\n");
		return err;
	}
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
	args.thresh = 100*1000*1000;	/* 50ms */
	globEv.stackfd = bpf_map__fd(unity_nosched->maps.stackmap);
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
	globEv.bhead = globEv.fhead = 0;
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
	unity_set_value(line, 2, "gt50ms", delta(summary,less100ms));
	unity_set_value(line, 3, "gt100ms", delta(summary,less500ms));
	unity_set_value(line, 4, "gt500ms", delta(summary,less1s));
	unity_set_value(line, 5, "gt1s", delta(summary,plus1s));
	prev = summary;
	flush_to_file(&globEv, -1);
	return 0;
}

void deinit(void)
{
	printf("unity_nosched plugin uninstall.\n");
	if (ksyms)
		free(ksyms);
	DESTORY_SKEL_BOJECT(unity_nosched);
}
