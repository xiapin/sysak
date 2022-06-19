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

int stk_fd;
extern volatile sig_atomic_t exiting;
extern struct ksym *ksyms;
void stamp_to_date(__u64 stamp, char dt[], int len);
void print_stack(int fd, __u32 ret, struct ksym *syms, FILE *fp);
static int stack_fd;

void handle_event_nosch(void *ctx, int cpu, void *data, __u32 data_sz)
{
	const struct event *e = data;
	char ts[64];

	stamp_to_date(e->stamp, ts, sizeof(ts));
	fprintf(fp_nsc, "%-21s %-5d %-15s %-8d %-10llu\n",
		ts, e->cpuid, e->task, e->pid, e->delay/(1000*1000));
	print_stack(stack_fd, e->ret, ksyms, fp_nsc);
}

void nosched_handler(int poll_fd)
{
	int err = 0;
	struct perf_buffer *pb = NULL;
	struct perf_buffer_opts pb_opts = {};

	fprintf(fp_nsc, "%-21s %-5s %-15s %-8s %-10s\n",
		"TIME(nosched)", "CPU", "COMM", "TID", "LAT(ms)");

	pb_opts.sample_cb = handle_event_nosch;
	pb = perf_buffer__new(poll_fd, 64, &pb_opts);
	if (!pb) {
		err = -errno;
		fprintf(stderr, "failed to open perf buffer: %d\n", err);
		goto clean_nosched;
	}

	while (!exiting) {
		err = perf_buffer__poll(pb, 100);
		if (err < 0 && err != -EINTR) {
			fprintf(stderr, "error polling perf buffer: %s\n", strerror(-err));
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
