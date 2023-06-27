#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <limits.h>
#include <argp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/types.h>	/* for __u64 */
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include "public.h"
#include "msr.h"
#include "cpuid.h"
/**
 * check:  CPUID.(EAX=07H, ECX=0):EBX.PQM[bit 12] reports 1
 * 
 */

int maxRMID;
long nr_cpu;
__u64 l3_factor;
struct env {
	bool verbose;
	bool local, total, cache, percpu;
} env = {
	.verbose = false,
	.local = false,
	.cache = false,
	.total = true,
	.percpu = false,
};
#define FAIL	(-1)

void *threadFunc(void *cookie)
{
	char array[1024*1024];
	__u64 old, new;
	msr_t *msr = (msr_t*)cookie;
	//printf("%d\n", rmid);
	set_msr_assoc(msr, msr->rmid);
	sleep(1);
	old = extract_val(read_mb_local(msr))*l3_factor;
	memset(array, 'a', sizeof(array));
	new = extract_val(read_mb_local(msr))*l3_factor;
	printf("rmid%-4u, old=%-12llu, new=%-12llu, delta=%-12llu\n",
		msr->rmid, old, new, new-old);
}

int collect(struct env *env, msr_t *msr)
{
	int i;
	char array[1024*1024];
	__u64 oldc, oldl, oldt, newc, newl, newt;

	oldc = oldl = oldt = newc = newl = newt = 0;
	printf("msr->rmid=%d\n", msr->rmid);
	sleep(1);
	for (i = 0; i < nr_cpu; i++) {
		set_msr_assoc(&msr[i], msr[i].rmid);
		if (env->cache)
			oldc += extract_val(read_l3_cache(&msr[i]))*l3_factor;
		if (env->local)
			oldl += extract_val(read_mb_local(&msr[i]))*l3_factor;
		if (env->total)
			oldt += extract_val(read_mb_total(&msr[i]))*l3_factor;
	}

	memset(array, 'a', sizeof(array));
	sleep(1);

	for (i = 0; i < nr_cpu; i++) {
	if (env->cache) {
		newc += extract_val(read_l3_cache(&msr[i]))*l3_factor;
	}
	if (env->local) {
		newl += extract_val(read_mb_local(&msr[i]))*l3_factor;
	}
	if (env->total) {
		newt += extract_val(read_mb_total(&msr[i]))*l3_factor;
	}
	}
		printf("cacheOc, old=%-12llu, new=%-12llu, delta=%-12llu\n",
			oldc, newc, newc-oldc);
		printf("localMBM, old=%-12llu, new=%-12llu, delta=%-12llu\n",
			oldl, newl, newl-oldl);
		printf("totalMBM, old=%-12llu, new=%-12llu, delta=%-12llu\n",
			oldt, newt, newt-oldt);
}

typedef void *(*start_routine) (void *);
int percpu_threads(start_routine thread_fun, void *cookie)
{
	int i;
	pthread_t *pth;
	pthread_attr_t attr;
	cpu_set_t cpu_info;
	msr_t *msrs = (msr_t *)cookie;

	pth = calloc(nr_cpu, sizeof(pthread_t));
	if (!pth)
		return -errno;
	for (i = 0; i < nr_cpu; i++) {
		pthread_attr_init(&attr);
		CPU_ZERO(&cpu_info);
		CPU_SET(i, &cpu_info);
		if (0!=pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpu_info)) {
			printf("pthread_attr_setaffinity_np fail:: cpu%d \n", i);
			continue;
		}
		if (0!=pthread_create(&pth[i], &attr, thread_fun, (void *)(&msrs[i]))) {
			printf("pthread_create fail: cpu%d\n", i);
			continue;
		}
	}
	for (i = 0; i < nr_cpu; i++)
		pthread_join(pth[i], NULL);
}

const char *argp_program_version = "cmtcat 0.1";
const char argp_program_doc[] =
"Catch the MBM and the CAT.\n"
"\n"
"USAGE: cmtcat [--help] [-t THRESH(ms)] [-S SHM] [-f LOGFILE] [duration(s)]\n"
"\n"
"EXAMPLES:\n"
"    cmtcat          # run forever, and detect cmtcat more than 10ms(default)\n"
"    cmtcat -l       # cat memory bandwidth of local\n"
"    cmtcat -t       # detect cmtcat with threshold 15ms (default 10ms)\n"
"    cmtcat -c       # record result to a.log (default to ~sysak/cmtcat/cmtcat.log)\n";

static const struct argp_option opts[] = {
	{ "local",  'l', "NULL", 0, "Local memory bandwidth"},
	{ "total",  't', "NULL", 0, "Total memory bandwidth"},
	{ "cache",  'c', "NULL", 0, "L3 cache Ocu" },
	{ "cache",  'p', "NULL", 0, "percpu data" },
	{ "verbose", 'v', NULL, 0, "Verbose debug output" },
	{ NULL, 'h', NULL, OPTION_HIDDEN, "Show the full help" },
	{},
};

static error_t parse_arg(int key, char *arg, struct argp_state *state)
{
	char *p;
	int ret = errno;
	static int pos_args;

	switch (key) {
	case 'h':
		argp_state_help(state, stderr, ARGP_HELP_STD_HELP);
		break;
	case 'l':
		env.local = true;
		break;
	case 't':
		env.total = true;
		break;
	case 'c':
		env.cache = true;
		break;
	case 'v':
		env.verbose = true;
		break;
	case 'p':
		env.percpu = true;
		break;
	case ARGP_KEY_ARG:
		if (pos_args++) {
			fprintf(stderr,
				"unrecognized positional argument: %s\n", arg);
			argp_usage(state);
		}
		errno = 0;
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	long nr_fd;
	msr_t *msrs;
	int *rmid;
	int i, ret, maxRMID;
	static const struct argp argp = {
		.options = opts,
		.parser = parse_arg,
		.doc = argp_program_doc,
	};

	if (!get_cpuid_maxleaf())
		return FAIL;
	if (!cpid_PQM_supported())
		return FAIL;
	if (!cpuid_L3_type_supported())
		return FAIL;
	maxRMID = cpuid_L3_RMID_eventID();
	if (maxRMID < 0)
		return FAIL;
	if((ret=get_cpus(&nr_cpu)) || nr_cpu < 0)
		return -ret;

	ret = argp_parse(&argp, argc, argv, 0, NULL, NULL);
	if (ret) {
		fprintf(stderr, "argp_parse fail\n");
		return ret;
	}

	if (maxRMID < nr_cpu)
		nr_cpu = maxRMID;

	l3_factor = cpuid_L3_factor();
	nr_fd = init_msr(&msrs);
	if (!msrs || nr_fd < 0)
		return FAIL;
	if (env.percpu) {
		for (i = 0; i < nr_cpu; i++)
			msrs[i].rmid = maxRMID-i;
		percpu_threads(threadFunc, msrs);
	} else {
		for (i = 0; i < nr_cpu; i++)
			msrs[i].rmid = 8;
		collect(&env, msrs);
	}
over:
	deinit_msr(msrs, nr_fd);
}

