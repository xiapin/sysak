#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include "nosched.h"
#include "schedmoni.h"

#define MAX_SYMS 300000
extern FILE *fp_nsc;
extern struct env env;

int stk_fd;
extern volatile sig_atomic_t exiting;
extern struct ksym *ksyms;
static int stack_fd;

static char stack[512];
void handle_event_nosch(void *ctx, int cpu, void *data, __u32 data_sz)
{
	const struct event *e = data;
	char ts[64];

	stamp_to_date(e->stamp, ts, sizeof(ts));
	if (env.mod_json) {
		int cnt;
		struct summary *sum;
		cJSON *arryItem;
		char current[32] = {0};

		sum = &env.summary[NOSCH];
		arryItem = cJSON_CreateObject();
		cJSON_AddStringToObject(arryItem, "date", ts);
		cJSON_AddStringToObject(arryItem, "class", "sys延迟");
		cJSON_AddNumberToObject(arryItem, "latency", e->delay/(1000*1000));
		cJSON_AddNumberToObject(arryItem, "cpu", e->cpuid);
		snprintf(current, sizeof(current), "%s (%d)", e->task, e->pid);
		cJSON_AddStringToObject(arryItem, "current", current);
		cJSON_AddNumberToObject(arryItem, "stamp", e->stamp);

		if (!e->exit) {
			cnt = print_stack(stack_fd, e->ret, 7, ksyms, stack, MOD_STRING);
			*(stack+cnt) = '\0';
		} else {
			sum->delay += e->delay;
			sum->cnt++;
			if (e->delay > sum->max)
				sum->max = e->delay;
			snprintf(stack, sizeof(stack), "%s", "(EOF)");
		}
		cJSON_AddStringToObject(arryItem, "extern", stack);
		cJSON_AddItemToArray(env.json.tbl_data, arryItem);
	} else {
		fprintf(fp_nsc, "%-21llu %-5d %-15s %-8d %-10llu %s\n",
			e->stamp, e->cpuid, e->task, e->pid, e->delay/(1000*1000),
			(e->exit==e->stamp)?"(EOF)":"");
		if (!e->exit)
			print_stack(stack_fd, e->ret, 7, ksyms, fp_nsc, MOD_FILE);
		fflush(fp_nsc);
	}
}

void handle_lost_nosch_events(void *ctx, int cpu, __u64 lost_cnt)
{
	printf("Lost %llu events on CPU #%d!\n", lost_cnt, cpu);
}

void nosched_handler(int poll_fd)
{
	int err = 0;
	struct perf_buffer *pb = NULL;
	struct perf_buffer_opts pb_opts = {};

	fprintf(fp_nsc, "%-21s %-5s %-15s %-8s %-10s\n",
		"TIME(nosched)", "CPU", "COMM", "TID", "LAT(ms)");

	pb_opts.sample_cb = handle_event_nosch;
	pb_opts.lost_cb = handle_lost_nosch_events;
	pb = perf_buffer__new(poll_fd, 64, &pb_opts);
	if (!pb) {
		err = -errno;
		fprintf(stdout, "failed to open perf buffer: %d\n", err);
		goto clean_nosched;
	}

	while (!exiting) {
		err = perf_buffer__poll(pb, 100);
		if (err < 0 && err != -EINTR) {
			fprintf(stdout, "error polling perf buffer: %s\n", strerror(-err));
			goto clean_nosched;
		}
		/* reset err to return 0 if exiting */
		err = 0;
	}

clean_nosched:
	perf_buffer__free(pb);
}

void *runnsc_handler(void *arg)
{
	struct tharg *runnsc = (struct tharg *)arg;

	stack_fd = runnsc->ext_fd;
	nosched_handler(runnsc->map_fd);

	return NULL;
}
