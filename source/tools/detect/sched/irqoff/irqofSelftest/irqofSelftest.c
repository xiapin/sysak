#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <linux/types.h>
#include <sys/types.h>
#include "sched_jit.h"

int begin, end;
char buffer[4096];
void *shm_handler(void *arg)
{
	int i;
	struct sched_jit_summary *sump;
	sump = (struct sched_jit_summary *)arg;

	while(!begin) {
		sleep(1);
	};

	while(!end) {
	for (i = 3; i >= 0; i--) {
		struct jit_maxN *maxN;
		struct jit_lastN *lastN;

		lastN = &sump->lastN_array[i];
		maxN = &sump->maxN_array[i];
		printf("**MAX:delay=%lld stamp=%lld cpu=%d pid=%d task=%s\n",
			maxN->delay, maxN->stamp, maxN->cpu,
			maxN->pid, maxN->comm); 
		printf("**LAST:cpu=%ld container=%s\n",lastN->cpu, lastN->con); 
	}
	printf("--num=%ld, total=%lld, <10ms=%ld, <50ms=%ld, <100ms=%ld <500ms=%ld <1s=%ld >1s=%ld\n\n",
		sump->num, sump->total, sump->less10ms, sump->less50ms,
		sump->less100ms, sump->less500ms, sump->less1s, sump->plus1s);
	sleep(2);
	}
}

int main(int argc, char *argv[])
{
	char *p;
	pthread_t pt;
	int i, ret, err, fd;
	FILE *fp;

	end = 0;
	begin = 0;
	fd = shm_open("irqofselftest", O_CREAT|O_RDWR|O_TRUNC, 06666);
	if (fd < 0) {
		ret = errno;
		perror("popen irqoff");
		return ret;
	}
	ftruncate(fd, sizeof(struct sched_jit_summary) + 32);
	p = mmap(NULL, sizeof(struct sched_jit_summary) + 32,
		PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

	err = pthread_create(&pt, NULL, shm_handler, p);
	if (err) {
		fprintf(stderr, "can't pthread_create: %s\n", strerror(errno));
		goto cleanup;
	}

	fp = popen("sysak irqoff -S irqofselftest -t 2 20", "r");
	if (!fp) {
		perror("popen irqoff");
		return -1;
	}
	sleep(1);
	begin = 1;

	ret = pclose(fp);
	if (ret == -1) {
		perror("pclose irqoff");
		return -1;
	}
	end = 1;
cleanup:
	munmap(p, sizeof(struct sched_jit_summary) + 32);
	ret = shm_unlink("irqofselftest");
	if (ret < 0) {
		ret = errno;
		perror("shm_unlink");
		return ret;
	}
	return 0;
}
