#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>	/* bpf_obj_pin */
#include <getopt.h>
#include "bpf/workqlatency.skel.h"
#include "workqlatency.h"

__u64 period = DEF_TIME;
__u64 g_thresh = LAT_THRESH_NS;
bool verbose = false;
static struct ksym syms[MAX_SYMS];
static int sym_cnt;


char *log_dir = "/var/log/sysak/workqlatency";
char *runtime_data = "/var/log/sysak/workqlatency/runtime.log";
char *latency_data = "/var/log/sysak/workqlatency/latency.log";

struct option longopts[] = {
    { "time", no_argument, NULL, 't' },
	{ "threshold", no_argument, NULL, 'l' },
    { "help", no_argument, NULL, 'h' },
    { 0, 0, 0, 0},
};

static void sig_int(int signo) { }

static void usage(void)
{
	fprintf(stdout,
	        "Usage: sysak workqlatency [options] [args]\n"
            "Options:\n"
            "    --time/-t		specify the monitor period(s), default=10s\n"
			"    --threshold/-l		specify the threshold time(ms), default=10ms\n"
            "    --help/-h		help info\n"
            "example: sysak workqlatency -t 10  #trace work runtime and latency statistics\n");
	exit(EXIT_FAILURE);
}

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
	if (verbose)
		return vfprintf(stderr, format, args);
	else
		return 0;
}

static void bump_memlock_rlimit(void)
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

static int ksym_cmp(const void *p1, const void *p2)
{
	return ((struct ksym *)p1)->addr - ((struct ksym *)p2)->addr;
}

int load_kallsyms(void)
{
	FILE *f = fopen("/proc/kallsyms", "r");
	char func[MAX_BUF], buf[MAX_BUF];
	char symbol;
	void *addr;
	int i = 0;

	if (!f)
		return -ENOENT;

	while (!feof(f)) {
		if (!fgets(buf, sizeof(buf), f))
			break;
		if (sscanf(buf, "%p %c %s", &addr, &symbol, func) != 3)
			break;
		if (!addr)
			continue;
		syms[i].addr = (long) addr;
		syms[i].name = strdup(func);
		i++;
	}
	fclose(f);
	sym_cnt = i;
	qsort(syms, sym_cnt, sizeof(struct ksym), ksym_cmp);
	return 0;
}

struct ksym *ksym_search(long key)
{
	int start = 0, end = sym_cnt;
	int result;

	/* kallsyms not loaded. return NULL */
	if (sym_cnt <= 0)
		return NULL;

	while (start < end) {
		size_t mid = start + (end - start) / 2;

		result = key - syms[mid].addr;
		if (result < 0)
			end = mid;
		else if (result > 0)
			start = mid + 1;
		else
			return &syms[mid];
	}

	if (start >= 1 && syms[start - 1].addr < key &&
	    key < syms[start].addr)
		/* valid ksym */
		return &syms[start - 1];

	/* out of range. return _stext */
	return &syms[0];
}

static void print_ksym(__u64 addr, char work_name[])
{
	struct ksym *sym;

	if (!addr)
		return;

	sym = ksym_search(addr);
	snprintf(work_name, MAX_SYMS, "<0x%llx> %s",
             addr, sym->name);
}

void stamp_to_date(__u64 stamp, char dt[], int len)
{
	time_t t, diff, last;
	struct tm *tm;
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	last = time(&t);
	if (stamp) {
		diff = ts.tv_sec * SEC_TO_NS + ts.tv_nsec - stamp;
		diff = diff / SEC_TO_NS;
		last = t - diff;
	}
	tm = localtime(&last);
	strftime(dt, len, "%F %H:%M:%S", tm);
}

static int prepare_dictory(char *path)
{
	int ret = 0;

	if (access(path,0) != 0)
        ret = mkdir(path, 0777);

	return ret;
}

/*static int get_work_name_from_map(struct work_key *key, char **ret_name)
{
	char name[MAX_KWORKNAME] = { 0 };
	int fd_name = bpf_map__fd(skel->maps.perf_kwork_names);

	*ret_name = NULL;

	if (fd_name < 0) {
		fprintf(stdout, "Invalid names map fd\n");
		return 0;
	}

	if ((bpf_map_lookup_elem(fd_name, key, name) == 0) && (strlen(name) != 0)) {
		*ret_name = strdup(name);
		if (*ret_name == NULL) {
			fprintf(stderr, "Failed to copy work name\n");
			return -1;
		}
	}

	return 0;
}*/

static int report_data(struct workqlatency_bpf *skel, enum trace_class_type type)
{
	int err = 0,fd_report;
	struct work_key prev, key;
	struct report_data elem_rd;
	char date_start[MAX_DATE], date_end[MAX_DATE];
	char workname[MAX_SYMS] = {0};
	FILE *fp_data;

	if (type == TRACE_RUNTIME){
		fd_report = bpf_map__fd(skel->maps.runtime_kwork_report);
    	printf("%-48s\t%-10s\t%-10s\t%-10s\t%-12s\t%-16s\t%-16s\n", "Kwork Name", "Cpu",
				"Avg runtime(ns)", "Count", "Max runtime(ns)", "Max runtime start(s)", "Max runtime end(s)");
	
		if (access(runtime_data,0) != 0){
			fp_data = fopen(runtime_data, "a+");
			fprintf(fp_data, "%-42s\t%-10s\t%-10s\t%-12s\t%-12s\t%-32s\t%-16s\n", "Kwork Name", "Cpu",
					"Avg runtime(ns)", "Count", "Max runtime(ns)", "Max runtime start(s)", "Max runtime end(s)");
		} else {
			fp_data = fopen(runtime_data, "a+");
		}
	} else {
		fd_report = bpf_map__fd(skel->maps.latency_kwork_report);
    	printf("%-48s\t%-10s\t%-10s\t%-10s\t%-12s\t%-16s\t%-16s\n", "Kwork Name", "Cpu",
				"Avg delay(ns)", "Count", "Max delay(ns)", "Max delay start(s)", "Max delay end(s)");
	
		if (access(latency_data,0) != 0){
			fp_data = fopen(latency_data, "a+");
			fprintf(fp_data, "%-48s\t%-10s\t%-10s\t%-12s\t%-12s\t%-32s\t%-16s\n", "Kwork Name", "Cpu",
					"Avg delay(ns)", "Count", "Max delay(ns)", "Max delay start(s)", "Max delay end(s)");
		} else {
			fp_data = fopen(latency_data, "a+");
		}

	}

	while (!bpf_map_get_next_key(fd_report, &prev, &key)) {
		err = bpf_map_lookup_elem(fd_report, &key, &elem_rd);
		if (err < 0) {
			fprintf(stderr, "failed to lookup elem: %d\n", err);
			goto out;
		}
		
		stamp_to_date(elem_rd.max_time_start, date_start,MAX_DATE);
		stamp_to_date(elem_rd.max_time_end, date_end, MAX_DATE);
		print_ksym(elem_rd.name_addr,workname);
		if (elem_rd.nr > 0){
			printf("%-48s\t%-10d\t%-10llu\t%-12llu\t%-12llu\t%s\t%s\n", workname, key.cpu,
				elem_rd.total_time/elem_rd.nr, elem_rd.nr, elem_rd.max_time, date_start, date_end);
			fprintf(fp_data, "%-48s\t%-10d\t%-10llu\t%-12llu\t%-12llu\t%s(%-16llu)\t%s(%-16llu)\n", workname, key.cpu,
				elem_rd.total_time/elem_rd.nr, elem_rd.nr, elem_rd.max_time, date_start, elem_rd.max_time_start,
				date_end, elem_rd.max_time_end);
		}
		prev = key;
	}
out:
	fclose(fp_data);
	return err;
}

int main(int argc, char *argv[])
{
	int opt;
	int err = 0, map_fd, args_key = 0;
	struct workqlatency_bpf *skel;
	struct args args;

	while ((opt = getopt_long(argc, argv, "t:l:hv", longopts, NULL)) != -1) {
		switch (opt) {
			case 't':
                if (optarg)
                    period = (int)strtoul(optarg, NULL, 10);
                break;
			case 'l':
                if (optarg)
                    g_thresh = (int)strtoul(optarg, NULL, 10)*1000*1000;
				break;
			case 'h':
                usage();
                break;
			case 'v':
                verbose = true;
                break;
			default:
                printf("must have parameter\n");
                usage();
                break;
        }
	}

	libbpf_set_print(libbpf_print_fn);
	bump_memlock_rlimit();

	prepare_dictory(log_dir);

	err = load_kallsyms();
	if (err) {
		fprintf(stderr, "Failed to load kallsyms\n");
		return err;
	}

	skel = workqlatency_bpf__open();
	if (!skel) {
		fprintf(stderr, "Failed to open BPF skeleton\n");
		return 1;
	}

	err = workqlatency_bpf__load(skel);
	if (err) {
		fprintf(stderr, "Failed to load BPF skeleton, errno:%d\n",err);
		return 1;
	}
	if (signal(SIGINT, sig_int) == SIG_ERR) {
		fprintf(stderr, "can't set signal handler: %s\n", strerror(errno));
		err = 1;
		goto cleanup;
	}

	err = workqlatency_bpf__attach(skel);
	if (err) {
		fprintf(stderr, "failed to attach BPF skeleton\n");
		goto cleanup;
	}

	map_fd = bpf_map__fd(skel->maps.args_map);

	args.thresh = g_thresh;
	err = bpf_map_update_elem(map_fd, &args_key, &args, 0);
	if (err) {
		fprintf(stderr, "Failed to update flag map\n");
		goto cleanup;
	}

	printf("Starting trace, Can hit <Ctrl+C> to abort and report\n");
	sleep(period);
	err = report_data(skel,	TRACE_LATENCY);
	err = report_data(skel, TRACE_RUNTIME);

cleanup:
	workqlatency_bpf__destroy(skel);
	return err;
}