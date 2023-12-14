#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define BUF_LEN 128

#define PID_MAX "/proc/sys/kernel/pid_max"
#define THREADS_MAX "/proc/sys/kernel/threads-max"

int g_shmid;
unsigned long int *task_created;
pthread_mutex_t *g_mutex;

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

int check_rlimit(int max)
{
	struct rlimit limit;

	if (getrlimit(RLIMIT_NPROC, &limit))
		return -1;

	if (limit.rlim_cur < max) {
		limit.rlim_cur = max;
		if (limit.rlim_max < max)
			limit.rlim_max = max;

		return setrlimit(RLIMIT_NPROC, &limit);
	}

	return 0;
}

void *thread_func(void *arg)
{
	while(1) {
		/*just sleep*/
		sleep(5);
	}
}

int create_threads(int nr)
{
	int i, err;
	pthread_attr_t attr;
	pthread_t *thread;
	void *ret;

	pthread_attr_init(&attr);
	thread = (pthread_t *)malloc(nr * sizeof(pthread_t));

	for (i = 0; i < nr; i++) {
		err = pthread_create(&thread[i], &attr, &thread_func, NULL);
		if (err)
			break;
	}

	*task_created = i;
	pthread_mutex_unlock(g_mutex);
	nr = i;
	printf("created %d threads in child\n", nr);
	for (i = 0; i < nr; i++) {
		pthread_join(thread[i], &ret);
	}

	exit(-1);
}

int create_tasks(unsigned long int nr)
{
	int ret, pid;
	unsigned long int created = 0;

	pthread_mutex_lock(g_mutex);
	while(nr > 0) {
		pid = fork();
		if (pid < 0) {
			break;
		} else if (pid == 0) {
			void *shaddr = shmat(g_shmid, NULL, 0);

			if (!shaddr) {
				perror("shmat");
				exit(-1);
			}

			g_mutex = (pthread_mutex_t *)shaddr;
			task_created = (unsigned long *)(shaddr + sizeof(pthread_mutex_t));
			create_threads(nr);
		} else {
			pthread_mutex_lock(g_mutex);
			created += *task_created;
			nr -= *task_created;
		}
	}

	return created > 0 ? 0 : -1;
}

int main(int argc ,char *argv[])
{
	int ret, nr = 0;
	void *shaddr;
	pthread_mutexattr_t attr;

	if (argc == 2)
		nr = atoi(argv[1]);

	if (nr <= 0)
		nr = get_system_process_limit();

	if (nr <= 0)
		return -1;

	if (check_rlimit(nr))
		return -1;

	g_shmid = shmget(IPC_PRIVATE, 4096, IPC_CREAT);
	if (g_shmid == -1) {
		perror("shmget");
		return -1;
	}
	shaddr = shmat(g_shmid, NULL, 0);
	if (!shaddr)
		return -1;

	g_mutex = (pthread_mutex_t *)shaddr;
	task_created = (unsigned long *)(shaddr + sizeof(pthread_mutex_t));

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, 1);
	pthread_mutex_init(g_mutex, &attr);
	printf("Will create %d tasks to limit\n", nr);

	return create_tasks(nr);
}
