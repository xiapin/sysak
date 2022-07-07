#include <argp.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sys/resource.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include "schedmoni.h"

extern FILE *fp_rsw;
extern volatile sig_atomic_t exiting;
static int previous, th_ret;
extern struct env env;

char exdatas[64];
void handle_event(void *ctx, int cpu, void *data, __u32 data_sz)
{
	const struct event *e = data;
	char ts[64];

	stamp_to_date(e->stamp, ts, sizeof(ts));
	previous = env.previous;
	if (env.mod_json) {
		int cnt;
		struct summary *sum;
		cJSON *arryItem;
		char *pos, current[32] = {0};

		sum = &env.summary[RQSLW];
		arryItem = cJSON_CreateObject();
		cJSON_AddStringToObject(arryItem, "date", ts);
		cJSON_AddStringToObject(arryItem, "class", "调度延迟");
		cJSON_AddNumberToObject(arryItem, "latency", e->delay/(1000*1000));
		cJSON_AddNumberToObject(arryItem, "cpu", e->cpuid);
		snprintf(current, sizeof(current), "%s (%d)", e->task, e->pid);
		cJSON_AddStringToObject(arryItem, "current", current);
		cJSON_AddNumberToObject(arryItem, "stamp", e->stamp);

		pos = exdatas;
		cnt = snprintf(pos, sizeof(exdatas), "nr_running=%d, prev=%s (%d)",
				e->rqlen, e->prev_task, e->prev_pid);
		if (cnt < 64)
			*(pos+cnt) = '\0';
		cJSON_AddStringToObject(arryItem, "extern", pos);
		cJSON_AddItemToArray(env.json.tbl_data, arryItem);
		sum->delay += e->delay;
		sum->cnt++;
		if (e->delay > sum->max)
			sum->max = e->delay;
	} else {
		if (env.previous)
			fprintf(fp_rsw, "%-21s %-5d %-15s %-8d %-10llu %-7d %-16s %-6d\n", ts, e->cpuid, e->task,
				e->pid, e->delay/(1000*1000), e->rqlen, e->prev_task, e->prev_pid);
		else
			fprintf(fp_rsw, "%-21s %-5d %-15s %-8d %-10llu %-7d\n", ts, e->cpuid, e->task, e->pid,
				e->delay/(1000*1000), e->rqlen);
	}
}

void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt)
{
	printf("Lost %llu events on CPU #%d!\n", lost_cnt, cpu);
}

void *runslw_handler(void *arg)
{
	int i = 0, err = 0;
	struct args bpf_args;
	struct tharg *data = (struct tharg *)arg;
	struct perf_buffer *pb = NULL;
	struct perf_buffer_opts pb_opts = {};

	previous = env.previous;
	if (!env.mod_json) {
		if (env.previous)
			fprintf(fp_rsw, "%-21s %-5s %-15s %-8s %-10s %-7s %-16s %-6s\n", "TIME(runslw)", "CPU", "COMM", "TID", "LAT(ms)", "RQLEN", "PREV COMM", "PREV TID");
		else
			fprintf(fp_rsw, "%-21s %-5s %-15s %-8s %-10s %-7s\n", "TIME(runslw)", "CPU", "COMM", "TID", "LAT(ms)", "RQLEN");
	}
	pb_opts.sample_cb = handle_event;
	pb = perf_buffer__new(data->map_fd, 128, &pb_opts);
	if (!pb) {
		err = -errno;
		fprintf(stderr, "failed to open perf buffer: %d\n", err);
		goto clean_runslw;
	}

	memset(&bpf_args, 0, sizeof(bpf_args));
	bpf_map_lookup_elem(data->ext_fd, &i, &bpf_args);
	bpf_args.ready = true;
	err = bpf_map_update_elem(data->ext_fd, &i, &bpf_args, 0);
	if (err) {
		fprintf(stderr, "Failed to update flag map\n");
		goto clean_runslw;
	}

	while (!exiting) {
		err = perf_buffer__poll(pb, 100);
		if (err < 0 && err != -EINTR) {
			fprintf(stderr, "error polling perf buffer: %s\n", strerror(-err));
			goto clean_runslw;
		}
		/* reset err to return 0 if exiting */
		err = 0;
	}

clean_runslw:
	perf_buffer__free(pb);
	th_ret = err;
	return &th_ret;
}
