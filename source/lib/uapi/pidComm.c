#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <error.h>
#include <errno.h>
#include <getopt.h>
#include <sys/types.h>

#define CONID_LEN 13
#define CGROUP_ROOT_PATH	"/sys/fs/cgroup/"
#define CG_PATH_LEN	512

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

/*
 *@subpath: get the path the container's cgroup path
 *@pid: the Pid of the container
 *@cg: the group, like cpuset, cpu, memory...
 * */
char buf_512[512];
static inline int get_con_cgpath_bypid(char *subpath, long pid, const char* cg, int alen)
{
	int ret = -1;
	FILE *fp;
	char cgroup_path[64] = {0};

	snprintf(cgroup_path, sizeof(cgroup_path), "/proc/%ld/cgroup", pid);
	fp = fopen(cgroup_path, "r");
	if (!fp)
		return errno;

	memset(buf_512, 0, 1024);
	while(fgets(buf_512, 1024, fp)) {
		size_t len;
		char *token;
		if((token = strstr(buf_512, cg)) != NULL) {
			char *p;
			p = strchr(token, ':');
			if (!p)
				continue;
			strncpy(subpath, p+1, alen); /* skip ":" */
			len = strlen(subpath);
			subpath[len-1] = 0; 		/* skip the newline:\n */
			ret = 0;
		}
	}
	return ret;
}

static inline long get_con_pid(const char *con)
{
	long pid;
	FILE *result;
	char cmd[128], buffer[128];

	//cmd="docker inspect --format "{{ .State.Pid}}" b21c04e42a1"
	snprintf(cmd, sizeof(cmd), "docker inspect --format \"{{ .State.Pid}}\" %s", con);
	result = popen(cmd, "r");
	if (feof(result) || !fgets(buffer, sizeof(buffer), result))
		return 0;

	pid = strtol(buffer, NULL, 10);
	return pid;
}

/*
 *@name: container ID or Name
 *@cg  : the control group name; such as memory,cpu,cpuset...
 *@path: the result to save; if NULL will calloc new space
 *@len : the lenth of @path
 * Note: path should have enough space
 * */
static inline char *get_cgroup_path(const char *name, const char *cg, char *path, int len)
{
	int ret, index;
	long pid;

	pid = get_con_pid(name);
	if (pid <= 0) {
		printf("container % is invalid\n", name);
		return NULL;
	}

	if (!path) {
		path = calloc(1, CG_PATH_LEN*sizeof(char));
		if (!path)
			return NULL;
		len = CG_PATH_LEN;
	}
	if (len < strlen(CGROUP_ROOT_PATH))
		return NULL;

	index = snprintf(path, len, "/sys/fs/cgroup/%s/", cg);
	ret = get_con_cgpath_bypid(path+index, pid, cg, len-index-1);
	if (!ret)
		return path;

	return NULL;
}

/* test cases */
#if 0
char *help_str = "self test";
static void usage(char *prog)
{
	const char *str =
	"  Usage: %s [OPTIONS]\n"
	"  Options:\n"
	"  -p PID               The pid we check\n"
	"  -c CONTAINER         The ID or name of container\n"
	;

	fprintf(stderr, str, prog);
	exit(EXIT_FAILURE);
}
struct environment {
	pid_t pid;
	char cgroup[32];
	size_t len;
	bool verbose;
} env = {
	.pid = 0,
	.cgroup = {0},
	.len = 32,
	.verbose = false,
};

int parse_args(int argc, char **argv, struct environment *env)
{
	int ret, c, option_index;

	ret = -EINVAL;
	for (;;) {
		c = getopt_long(argc, argv, "p:c:vh", NULL, &option_index);
		if (c == -1)
			break;
		switch (c) {
			case 'c':
				strncpy(env->cgroup, optarg, env->len);
				ret = 0;
				break;
			case 'p':
				env->pid = (int)strtol(optarg, NULL, 10);
				if ((errno == ERANGE && (env->pid == LONG_MAX || env->pid == LONG_MIN))
					|| (errno != 0 && env->pid == 0)) {
					perror("strtoul");
					ret = errno;
					goto parse_out;
				}
				ret = 0;
				break;
			case 'v':
				ret = 0;
				env->verbose = true;
				break;
			case 'h':
				usage(help_str);	/* would exit */
				break;
			default:
				usage(help_str);
		}
	}

parse_out:
	return ret;
}

int main(int argc, char *argp[])
{
	int ret;
	char tmp[CONID_LEN] = {0};
	int pid = getpid();
	char *cgpath = NULL;

	parse_args(argc, argp, &env);

	if (env.pid)
		pid = env.pid;
	printf("TEST for get_container...\n");
	/* test for get_container: */
	ret = get_container(tmp, pid);
	if (!ret)
		printf(" Success... dockerid=%s\n", tmp);
	else if (ret == ENXIO)
		printf(" Failed ... pid %d is not in container\n", pid);
	else
		printf(" Failed ... pid %d %s\n", pid, strerror(ret));

	printf("TEST for get_cgroup_path...\n");
	cgpath = get_cgroup_path(env.cgroup, "cpu", NULL, 0);
	if (cgpath) {
		printf(" Succeess... cgroup path=%s\n", cgpath);
		free(cgpath);
	}
}
#endif
