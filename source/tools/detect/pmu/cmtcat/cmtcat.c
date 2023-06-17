#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
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
#define FAIL	(-1)

void *test(void *cookie)
{
	char array[1024*1024];
	__u64 old, new;
	msr_t *msr = (msr_t*)cookie;
	//printf("%d\n", rmid);
	set_msr_assoc(msr, msr->rmid);
	sleep(1);
	old = read_mb_local(msr)*l3_factor;
	memset(array, 'a', sizeof(array));
	new = read_mb_local(msr)*l3_factor;
	printf("rmid%u, old=%llu, new=%llu, delta=%llu\n",
		msr->rmid, old, new, new-old);
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

int main()
{
	int i, ret, maxRMID;
	long nr_fd;
	msr_t *msrs;
	int *rmid;

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

	if (maxRMID < nr_cpu)
		nr_cpu = maxRMID;
	nr_fd = init_msr(&msrs);
	if (!msrs || nr_fd < 0)
		return FAIL;

	l3_factor = cpuid_L3_factor();
	for (i = 0; i < nr_cpu; i++)
		msrs[i].rmid = maxRMID-i;
	percpu_threads(test, msrs);
over:
	deinit_msr(msrs, nr_fd);
}

