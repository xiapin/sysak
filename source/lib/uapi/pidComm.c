#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <error.h>
#include <errno.h>
#include <sys/types.h>

#define CONID_LEN 13
/*
 * EXAMPLE: get current's container-id
 *  char dockerid[CONID_LEN] = {0};
 *  int pid = getpid();
 *  get_container(dockerid, 1118);
 * RETURN VALUE: 0 success, or this "pid" is not in a container or other fail. 
 * */
int get_container(char *dockerid, int pid)
{
	char *buf;
	FILE *fp;
	int ret = -1;
	char cgroup_path[64] = {0};

	if (pid < 0)
		return ESRCH;

	snprintf(cgroup_path, sizeof(cgroup_path), "/proc/%d/cgroup", pid);

	fp = fopen(cgroup_path, "r");
	if (!fp)
		return errno;

	buf = malloc(4096*sizeof(char));
	if (!buf) {
		ret = errno;
		goto out2;
	}

	ret = ENXIO;	/* if pid not in a container,return -ENXIO */
	while(fgets(buf, 4096, fp)) {
		int stat = 0;
		char *token, *pbuf = buf;
		while((token = strsep(&pbuf, "/")) != NULL) {
			if (stat == 1) {
				stat++;
				break;
			}
			if (!strncmp("docker", token, strlen("docker")))
				stat++;
		}

		if (stat == 2) {
			strncpy(dockerid, token, CONID_LEN - 1);
			ret = 0;
			goto out1;
		}
	}
out1:
	free(buf);	
out2:
	fclose(fp);
	return ret;
}

/* test cases */
#if 0
int main(int argc, char *argp[])
{
	int ret;
	char tmp[CONID_LEN] = {0};
	int pid = getpid();

	/* test for get_container: */
	ret = get_container(tmp, pid);
	if (!ret)
		printf("dockerid=%s\n", tmp);
	else if (ret == ENXIO)
		printf("pid %d is not in container\n", pid);
	else
		printf("%s\n", strerror(ret));
}
#endif
