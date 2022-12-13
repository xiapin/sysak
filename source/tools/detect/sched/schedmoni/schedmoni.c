#include <argp.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <linux/version.h>
#include <sys/types.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include "schedmoni.h"
#include "bpf/schedmoni.skel.h"

extern int stk_fd;
extern struct ksym *ksyms;
extern struct epool events_pool;

int nr_cpus;
char filename[256] = {0};
volatile sig_atomic_t exiting = 0;
FILE *fp_nsc = NULL, *fp_rsw = NULL, *fp_irq = NULL;
char log_dir[] = "/var/log/sysak/schedmoni";
char rswf[] = "/var/log/sysak/schedmoni/runslow.log";
char nscf[] = "/var/log/sysak/schedmoni/nosched.log";
char irqf[] = "/var/log/sysak/schedmoni/irqoff.log";
char json_log[] = "/var/log/sysak/schedmoni/schedmoni.json";
char *mode_name[] = {"调度延迟", "sys延迟", "irq延迟"};
char *mode_cnt_str[] = {"调度延迟次数", "sys延迟次数", "irq延迟次数"};

struct env env = {
	.span = 0,
	.thresh = 50*1000*1000,
	.fp = NULL,
	.mod_json = false,
};

const char *argp_program_version = "schedmoni 0.1";
const char argp_program_doc[] =
"Trace schedule latency.\n"
"\n"
"USAGE: schedmoni [--help] [-s SPAN] [-t TID] [-c COMM] [-P] [-j|-f LOGFILE] [threshold]\n"
"\n"
"EXAMPLES:\n"
"  schedmoni          # trace latency higher than 50 ms (default)\n"
"  schedmoni -f a.log # result to a.log (default ~sysak/schedmoni/schedmoni.log)\n"
"  schedmoni 20       # trace latency higher than 20 ms\n"
"  schedmoni -p 123   # trace pid 123\n"
"  schedmoni -t 123   # trace tid 123 (use for threads only)\n"
"  schedmoni -c bash  # trace aplication who's name is bash\n"
"  schedmoni -s 10    # monitor for 10 seconds\n"
"  schedmoni -P       # also show previous task name and TID\n"
"  schedmoni -j       # record result as json,exclusive with -f\n";

static const struct argp_option opts[] = {
	{ "pid", 'p', "PID", 0, "Process PID to trace"},
	{ "tid", 't', "TID", 0, "Thread TID to trace"},
	{ "comm", 'c', "COMM", 0, "Name of the application"},
	{ "span", 's', "SPAN", 0, "How long to run"},
	{ "logfile", 'f', "LOGFILE", 0, "logfile for result"},
	{ "verbose", 'v', NULL, 0, "Verbose debug output" },
	{ "previous", 'P', NULL, 0, "also show previous task name and TID" },
	{ "json", 'j', NULL, 0, "record the result with JSON mode" },
	{ NULL, 'h', NULL, OPTION_HIDDEN, "Show the full help" },
	{},
};

int load_kallsyms(struct ksym **pksyms);
int attach_prog_to_perf(struct schedmoni_bpf *obj, struct bpf_link **sw_mlinks, struct bpf_link **hw_mlinks);
static void bump_memlock_rlimit(void)
{
	struct rlimit rlim_new = {
		.rlim_cur = RLIM_INFINITY,
		.rlim_max = RLIM_INFINITY,
	};

	if (setrlimit(RLIMIT_MEMLOCK, &rlim_new)) {
		fprintf(stderr, "Failed to increase RLIMIT_MEMLOCK limit!\n");
		exit(1);
	}
}

static inline void close_logfile(FILE *fp0, FILE *fp1, FILE *fp2)
{
	if (fp0)
		fclose(fp0);
	if (fp1)
		fclose(fp1);
	if (fp2)
		fclose(fp2);
}

static inline FILE* open_logfile(char *filename, FILE **fporig)
{
	FILE *fp = *fporig;

	/* if original FILE is opened, just skip... */
	if (fp)
		return fp;
	fp = fopen(filename, "w+");
	if (!fp)
		fprintf(stderr, "%s :fopen %s\n",
			strerror(errno), filename);
	return fp;
}

static int prepare_directory(char *path)
{
	int ret;

	ret = mkdir(path, 0777);
	if (ret < 0 && errno != EEXIST)
		return errno;
	else
		return 0;
}

static error_t parse_arg(int key, char *arg, struct argp_state *state)
{
	int pid;
	static int pos_args;
	long long thresh, span;

	switch (key) {
	case 'h':
		argp_state_help(state, stderr, ARGP_HELP_STD_HELP);
		break;
	case 'v':
		env.verbose = true;
		break;
	case 'P':
		env.previous = true;
		break;
	case 'j':
		env.mod_json = true;
		env.json.root = cJSON_CreateObject();
		env.json.datasources = cJSON_CreateObject();
		env.json.tbl = cJSON_CreateObject();
		env.json.tbl_data = cJSON_CreateArray();
		cJSON_AddItemToObject(env.json.tbl, "data", env.json.tbl_data);
		env.json.tms = cJSON_CreateObject();
		env.json.tms_data= cJSON_CreateArray();
		cJSON_AddItemToObject(env.json.tms, "data", env.json.tms_data);
		env.json.evt = cJSON_CreateObject();
		env.json.evt_data= cJSON_CreateArray();
		cJSON_AddItemToObject(env.json.evt, "data", env.json.evt_data);
		cJSON_AddItemToObject(env.json.datasources, "jitterEventSummary", env.json.evt);
		cJSON_AddItemToObject(env.json.datasources, "jitterTimeSeries", env.json.tms);
		cJSON_AddItemToObject(env.json.datasources, "jitterTable", env.json.tbl);
		cJSON_AddItemToObject(env.json.root, "datasources", env.json.datasources);
		break;
	case 'p':
		errno = 0;
		pid = strtol(arg, NULL, 10);
		if (errno || pid <= 0) {
			fprintf(stderr, "Invalid PID: %s\n", arg);
			argp_usage(state);
			return errno;
		}
		env.pid = pid;
		break;
	case 't':
		errno = 0;
		pid = strtol(arg, NULL, 10);
		if (errno || pid <= 0) {
			fprintf(stderr, "Invalid TID: %s\n", arg);
			argp_usage(state);
			return errno;
		}
		env.tid = pid;
		break;
	case 'c':
		env.comm.size = strlen(arg);
		if (env.comm.size < 1) {
			fprintf(stderr, "Invalid COMM: %s\n", arg);
			argp_usage(state);
			return -1;
		}

		if (env.comm.size > TASK_COMM_LEN - 1)
			env.comm.size = TASK_COMM_LEN - 1;

		strncpy(env.comm.comm, arg, env.comm.size);
		break;
	case 's':
		errno = 0;
		span = strtoul(arg, NULL, 10);
		if (errno || span <= 0) {
			fprintf(stderr, "Invalid SPAN: %s\n", arg);
			argp_usage(state);
			return errno;
		}
		env.span = span;
		break;
	case 'f':
		if (strlen(arg) > 1) {
			snprintf(filename, sizeof(filename), "%s.rswf", arg);
			fp_rsw = open_logfile(filename, &fp_rsw);

			memset(filename, 0, sizeof(filename));
			snprintf(filename, sizeof(filename), "%s.nscf", arg);
			fp_nsc = open_logfile(filename, &fp_nsc);

			memset(filename, 0, sizeof(filename));
			snprintf(filename, sizeof(filename), "%s.irqf", arg);
			fp_irq = open_logfile(filename, &fp_irq);
		}
		break;
	case ARGP_KEY_ARG:
		if (pos_args++) {
			fprintf(stderr,
				"Unrecognized positional argument: %s\n", arg);
			argp_usage(state);
		}
		errno = 0;
		thresh = strtoll(arg, NULL, 10);
		if (errno || thresh <= 0) {
			fprintf(stderr, "Invalid delay (in us): %s\n", arg);
			argp_usage(state);
		}
		env.thresh = thresh*1000*1000;
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
	if (level == LIBBPF_DEBUG && !env.verbose)
		return 0;
	return vfprintf(stdout, format, args);
}

static void sig_exiting(int signo)
{
	exiting = 1;
}

int check_kprobe(struct schedmoni_bpf *obj)
{
	int i, ret = 0;
	char *str, *endptr;
	unsigned long ver[3];
	struct utsname ut;

	ret = uname(&ut);
	if (ret < 0)
		return -errno;

	str = ut.release;
	for (i = 0; i < 3; i++) {
		ver[i] = strtoul(str, &endptr, 10);
		if ((errno == ERANGE && (ver[i] == LONG_MAX || ver[i] == LONG_MIN))
			|| (errno != 0 && ver[i] == 0)) {
			perror("strtol");
			return -errno;
		}
		str = endptr+1;
	}

	if (ver[0] >= 4 && ver[1] >= 19) {
		ret = bpf_program__set_autoload(obj->progs.kp_ttwu_do_wakeup, false);
		if (ret < 0) {
			printf("FAIL:bpf_program__set_autoload kp_ttwu_do_wakeup\n");
			return ret;
		}
	} else {
		bpf_program__set_autoload(obj->progs.raw_tp__sched_wakeup, false);
		if (ret < 0) {
			printf("FAIL:bpf_program__set_autoload raw_tp__sched_wakeup\n");
			return ret;
		}
	}

	return 0;
}

void handle_event(void *ctx, int cpu, void *data, __u32 data_sz);
void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt);
void *runslw_handler(void *arg);
void *runnsc_handler(void *arg);
void *irqoff_handler(void *arg);

int main(int argc, char **argv)
{
	void *res;
	int i, err;
	__u64 loops = 0;
	int arg_fd, map_rslw_fd, map_nsch_fd, map_irqf_fd;
	pthread_t pt_runslw, pt_runnsc, pt_irqoff;
	struct schedmoni_bpf *obj;
	struct args args = {};
	struct tharg runslw = {}, runnsc = {}, irqoff ={};
	struct bpf_link **sw_mlinks, **hw_mlinks= NULL;
	static const struct argp argp = {
		.options = opts,
		.parser = parse_arg,
		.doc = argp_program_doc,
	};

	bump_memlock_rlimit();

	err = prepare_directory(log_dir);
	if (err)
		return err;

	nr_cpus = libbpf_num_possible_cpus();
	if (nr_cpus < 0) {
		fprintf(stderr, "failed to get # of possible cpus: '%s'!\n",
			strerror(-nr_cpus));
		return 1;
	}

	memset(&env.comm, 0, sizeof(struct comm_item));
	fp_rsw = fp_nsc = fp_irq = NULL;
	err = argp_parse(&argp, argc, argv, 0, NULL, NULL);
	if (err)
		return err;

	libbpf_set_print(libbpf_print_fn);
	/* check again for no any arguments, in that case, FILEs are NULL */
	if (!fp_rsw || !fp_nsc || !fp_irq) {
		fp_rsw = open_logfile(rswf, &fp_rsw);
		fp_nsc = open_logfile(nscf, &fp_nsc);
		fp_irq = open_logfile(irqf, &fp_irq);
		if (!fp_rsw || !fp_nsc || !fp_irq) {
			close_logfile(fp_rsw, fp_nsc, fp_irq);
			return -EINVAL;
		}
	}

	ksyms = NULL;
	err = load_kallsyms(&ksyms);
	if (err) {
		fprintf(stderr, "Failed to load kallsyms\n");
		return err;
	}

	sw_mlinks = calloc(nr_cpus, sizeof(*sw_mlinks));
	if (!sw_mlinks) {
		fprintf(stderr, "failed to alloc sw_mlinks or rlinks\n");
		return -ENOMEM;
	}
	hw_mlinks = calloc(nr_cpus, sizeof(*hw_mlinks));
	if (!hw_mlinks) {
		fprintf(stderr, "failed to alloc hw_mlinks or rlinks\n");
		free(ksyms);
		free(sw_mlinks);
		return -ENOMEM;
	}

	obj = schedmoni_bpf__open();
	if (!obj) {
		fprintf(stderr, "failed to open BPF object\n");
		return 1;
	}
	/* Here we are, providing selection for users */
#if 0
	bpf_program__set_autoload(obj->progs.raw_tracepoint__sched_wakeup, false);
	bpf_program__set_autoload(obj->progs.raw_tracepoint__sched_wakeup_new, false);
	bpf_program__set_autoload(obj->progs.hw_irqoff_event, false);
	bpf_program__set_autoload(obj->progs.sw_irqoff_event1, false);
	bpf_program__set_autoload(obj->progs.sw_irqoff_event2, false);
#endif
	err = check_kprobe(obj);
	if (err) {
		fprintf(stderr, "failed to check kprobe: %d\n", err);
		goto cleanup;
	}
	err = schedmoni_bpf__load(obj);
	if (err) {
		fprintf(stderr, "failed to load BPF object: %d\n", err);
		goto cleanup;
	}

	i = 0;
	arg_fd = bpf_map__fd(obj->maps.argmap);
	map_rslw_fd = bpf_map__fd(obj->maps.events_rnslw);
	map_nsch_fd = bpf_map__fd(obj->maps.events_nosch);
	map_irqf_fd = bpf_map__fd(obj->maps.events_irqof);
	stk_fd = bpf_map__fd(obj->maps.stackmap);
	args.comm_i = env.comm;
	args.targ_tgid = env.pid;
	args.targ_pid = env.tid;
	args.thresh = env.thresh;
	args.flag = TIF_NEED_RESCHED;
	args.ready = false;

	err = bpf_map_update_elem(arg_fd, &i, &args, 0);
	if (err) {
		fprintf(stderr, "Failed to update flag map\n");
		goto cleanup;
	}

	if (signal(SIGINT, sig_exiting) == SIG_ERR ||
		signal(SIGALRM, sig_exiting) == SIG_ERR) {
		fprintf(stderr, "can't set signal handler: %s\n", strerror(errno));
		err = 1;
		goto cleanup;
	}

	runslw.map_fd = map_rslw_fd;
	runslw.ext_fd = arg_fd;
	err = pthread_create(&pt_runslw, NULL, runslw_handler, &runslw);
	if (err) {
		fprintf(stderr, "can't pthread_create runslw: %s\n", strerror(errno));
		goto cleanup;
	}

	runnsc.map_fd = map_nsch_fd;
	runnsc.ext_fd = stk_fd;
	err = pthread_create(&pt_runnsc, NULL, runnsc_handler, &runnsc);
	if (err) {
		fprintf(stderr, "can't pthread_create runnsc: %s\n", strerror(errno));
		goto cleanup;
	}

	irqoff.map_fd = map_irqf_fd;
	irqoff.ext_fd = stk_fd;
	err = pthread_create(&pt_irqoff, NULL, irqoff_handler, &irqoff);
	if (err) {
		fprintf(stderr, "can't pthread_create irqoff: %s\n", strerror(errno));
		goto cleanup;
	}

	if (env.span)
		alarm(env.span);

	err = attach_prog_to_perf(obj, sw_mlinks, hw_mlinks);
	if (err) {
		free(sw_mlinks);
		free(hw_mlinks);
		sw_mlinks = hw_mlinks = NULL;
	}

	err = schedmoni_bpf__attach(obj);
	if (err) {
		fprintf(stderr, "failed to attach BPF programs\n");
		goto cleanup;
	}
	if (env.mod_json) {
		struct summary prev[3], next[3], delta[3];
		memset(prev, 0, sizeof(prev));
		memset(next, 0, sizeof(next));
		memset(delta, 0, sizeof(delta));
		while(!exiting) {
			char date[32];
			bool addjson = false;
			cJSON *arryItem;

			stamp_to_date(0, date, sizeof(date));
			arryItem = cJSON_CreateObject();
			for (i = 0; i < MAX_MOD; i++) {
				next[i] = env.summary[i];
				delta[i].delay = next[i].delay - prev[i].delay;
				delta[i].cnt = next[i].cnt - prev[i].cnt;
				delta[i].max = next[i].max;
				/* This is for time-series table */
				if (delta[i].delay || delta[i].cnt) {
					if (!addjson) {
						cJSON_AddStringToObject(arryItem, "time", date);
						addjson = true;
					}
					cJSON_AddNumberToObject(arryItem, mode_name[i], delta->delay/(1000*1000));
					cJSON_AddNumberToObject(arryItem, mode_cnt_str[i], delta->cnt);
				}
				prev[i] = next[i];
			}
			if (addjson)
				cJSON_AddItemToArray(env.json.tms_data, arryItem);
			sleep(1);
			loops++;
		}
	}
	pthread_join(pt_runslw, &res);
	pthread_join(pt_runnsc, &res);
	pthread_join(pt_irqoff, &res);

	if (env.mod_json) {
		FILE *fp;
		char *out;
		struct summary sum[3];
		int color, max_cnt, thresh_rate;
		enum {RED = 0, BLUE, GREEN};
		char *colors[] = {"red", "blue", "green"};
		char *ev[] = {"异常", "告警", "正常"};
		cJSON *arryItem;

		for (i = 0; i < MAX_MOD; i++) {
			arryItem = cJSON_CreateObject();
			sum[i] = env.summary[i];
			max_cnt = (loops*SEC_TO_NS)/env.thresh;
			if (max_cnt == 0)
				thresh_rate = 0;
			else
				thresh_rate = sum[i].real_cnt*100/max_cnt;
			
			if (sum[i].max > 500*1000*1000 && sum[i].max > env.thresh*2)
				color = RED;
			else if (sum[i].max > env.thresh*2)
				color = BLUE;
			else if (thresh_rate > 20)
				color = RED;
			else if (thresh_rate > 5)
				color = BLUE;
			else
				color = GREEN;
			cJSON_AddStringToObject(arryItem, "key", mode_name[i]);
			cJSON_AddStringToObject(arryItem, "value", ev[color]);
			cJSON_AddStringToObject(arryItem, "color", colors[color]);
			cJSON_AddItemToArray(env.json.evt_data, arryItem);
		}

		fp = fopen(json_log, "w+");
		if (!fp) {
			fprintf(stdout, "%s :fopen %s\n",
				strerror(errno), json_log);
			fp = stderr;
		}
		out = cJSON_Print(env.json.root);
		fprintf(fp, "%s", out);
		free(out);
		cJSON_Delete(env.json.root);
	}
cleanup:
	for (i = 0; i < nr_cpus; i++) {
		bpf_link__destroy(sw_mlinks[i]);
		bpf_link__destroy(hw_mlinks[i]);
	}

	if (sw_mlinks)
		free(sw_mlinks);
	if (hw_mlinks)
		free(hw_mlinks);
	if (ksyms)
		free(ksyms);
	schedmoni_bpf__destroy(obj);
	return err != 0;
}
