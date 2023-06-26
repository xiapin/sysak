#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define BUF_LEN 128

#define PID_MAX "/proc/sys/kernel/pid_max"
#define THREADS_MAX "/proc/sys/kernel/threads-max"

void *thread_func(void *arg)
{
	while(1) {
		/*just sleep*/
		sleep(5);
	}
}

int get_system_process_limit(void)
{
	int pid_max = -1;
	int threads_max;
	FILE *fp;
	char buf[BUF_LEN];

	fp = fopen(PID_MAX, "r");
	if (!fp) {
		printf("open %s failed\n", PID_MAX);
		return -1;
	}

	memset(buf, 0, BUF_LEN);
	if (fgets(buf, BUF_LEN, fp)) {
		pid_max = atoi(buf);
	}

	fclose(fp);
	if (pid_max < 0)
		return -1;

	fp = fopen(THREADS_MAX, "r");
	if (!fp) {
		printf("open %s failed\n", THREADS_MAX);
		return -1;
	}

	memset(buf, 0, BUF_LEN);
	if (fgets(buf, BUF_LEN, fp)) {
		threads_max = atoi(buf);
	}

	fclose(fp);
	if (threads_max < 0)
		return -1;
	
	return pid_max < threads_max ? pid_max: threads_max;
}

int main(int argc ,char *argv[])
{
	int nr = 50, i;
	pthread_attr_t attr;
	pthread_t *thread;
	void *ret;

	nr = get_system_process_limit();

	if (nr <= 0)
		return -1;

	pthread_attr_init(&attr);
	thread = (pthread_t *)malloc(nr * sizeof(pthread_t));

	for (i = 0; i < nr; i++) {
		if (pthread_create(&thread[i], &attr, &thread_func, NULL) < 0) {
			printf("create thread failed\n");
			return -1;
		}
	}

	printf("create %d threads to limit\n", nr);

	for (i = 0; i < nr; i++) {
		pthread_join(thread[i], &ret);
	}

	return 0;
}
