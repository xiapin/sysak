/*
 * iostat.c for 2.6.* with file /proc/diskstat
 * Linux I/O performance monitoring utility
 */
#include "tsar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <fcntl.h>
#include <linux/major.h>
#include <stdbool.h>
#include <dirent.h>
#include "sched_jit.h"

char *cg_usage = "    --cg                Linux container stats";

/* FIX ME: 128 may not be enough for PODs environment */
#define MAX_CGROUPS 128
/*Todo,user configure ?*/
#define CGROUP_INFO_AGING_TIME  7200
#define NCONF_HW	4
#define JITITEM		4

int cg_jitter_init = 0;
int nr_cpus;
char *jit_shm_key = "sysak_mservice_jitter_shm";

enum jitter_types {
	RQSLOW,
	NOSCHE,
	IRQOFF,
	JITTER_NTYPE,
};

__u32 hwtypes[] = {
	PERF_TYPE_HARDWARE,
	PERF_TYPE_HARDWARE,
	PERF_TYPE_HW_CACHE,
	PERF_TYPE_HW_CACHE,
};

__u64 hwconfigs[] = {
	PERF_COUNT_HW_CPU_CYCLES,
	PERF_COUNT_HW_INSTRUCTIONS,
	PERF_COUNT_HW_CACHE_LL			<<  0  |
	(PERF_COUNT_HW_CACHE_OP_READ		<<  8) |
	(PERF_COUNT_HW_CACHE_RESULT_ACCESS	<< 16),
	PERF_COUNT_HW_CACHE_LL			<<  0  |
	(PERF_COUNT_HW_CACHE_OP_READ		<<  8) |
	(PERF_COUNT_HW_CACHE_RESULT_MISS	<< 16),
};

struct cg_load_info {
	int load_avg_1;
	int load_avg_5;
	int load_avg_15;
	int nr_running;
	int nr_uninterrupt;
};

struct cg_cpu_info {
	unsigned long long cpu_user;
	unsigned long long cpu_nice;
	unsigned long long cpu_sys;
	unsigned long long cpu_idle;
	unsigned long long cpu_iowait;
	unsigned long long cpu_hirq;
	unsigned long long cpu_sirq;
	unsigned long long cpu_steal;
	unsigned long long cpu_guest;
	unsigned long long nr_throttled;
	unsigned long long throttled_time;
};

struct cg_memlat_info {
	unsigned long long lat_1;
	unsigned long long lat_5;
	unsigned long long lat_10;
	unsigned long long lat_100;
	unsigned long long lat_500;
	unsigned long long lat_1s;
	unsigned long long lat_over1s;
	unsigned long long total_lat_cnt;
	unsigned long long total_lat_time;
};

struct cg_mem_info {
	unsigned long long cache;
	unsigned long long used;
	unsigned long long limit;
	unsigned long long free;
	unsigned long long util;
	unsigned long long pgfault;
	unsigned long long pgmjfault;
	unsigned long long failcnt;
	struct cg_memlat_info drgl;
	struct cg_memlat_info drml;
	struct cg_memlat_info dcl;
};

struct cg_blkio_info {
	unsigned long long rd_ios;  /* read i/o operations */
	unsigned long long rd_bytes;   /* read i/o bytes */
	unsigned long long rd_wait; /* read i/o wait time */
	unsigned long long rd_time;    /* read i/o service time */
	unsigned long long rd_qios;  /* read i/o queued operations */
	unsigned long long rd_qbytes;   /* read i/o queued bytes */
	unsigned long long wr_ios;  /* read i/o operations */
	unsigned long long wr_bytes;   /* read i/o bytes */
	unsigned long long wr_wait; /* read i/o wait time */
	unsigned long long wr_time;    /* read i/o service time */
	unsigned long long wr_qios;  /* read i/o queued operations */
	unsigned long long wr_qbytes;   /* read i/o queued bytes */
};

struct cg_hwres_info {
	int **hw_fds;
	long long **hw_counters;
	/*
 	 * PERF_COUNT_HW_CPU_CYCLES,
	 * PERF_COUNT_HW_INSTRUCTIONS,
	 * PERF_COUNT_LLC_R_CACHE_REF,
	 * PERF_COUNT_LLC_R_CACHE_MISS,
	*/
	long long hw_sum[NCONF_HW];
	long long CPI, deltaCPI;
};

struct jitter_info {
	int idx;
	unsigned long num;
	unsigned long long time;
	int lastcpu[JITITEM];
	char container[JITITEM][CONID_LEN];
};

struct cg_jitter_info {
	struct jitter_info info[JITTER_NTYPE];
};

struct cgroup_info {
	char name[66];
	int valid;
	struct cg_load_info load;
	struct cg_cpu_info cpu;
	struct cg_mem_info mem;
	struct cg_blkio_info blkio;
	struct cg_hwres_info hwres;
	struct cg_jitter_info jitter;
} cgroups[MAX_CGROUPS];

unsigned int n_cgs = 0;  /* Number of cgroups */
char buffer[256];       /* Temporary buffer for parsing */
char buf_1024[1024];       /* Temporary buffer for parsing */

struct jitter_shm {
	struct sched_jit_summary nosched;
	struct sched_jit_summary irqoff;
	struct sched_jit_summary rqslow;
};
static struct jitter_shm *jitshm = NULL;

static struct mod_info cg_info[] = {
	/* load info 0-4 */
	{" load1", HIDE_BIT,  0,  STATS_NULL},
	{" load5", HIDE_BIT,  0,  STATS_NULL},
	{"load15", HIDE_BIT,  0,  STATS_NULL},
	{"  nrun", HIDE_BIT,  0,  STATS_NULL},
	{"nunint", HIDE_BIT,  0,  STATS_NULL},
	/* cpu info 5-15 */
	{"  user", DETAIL_BIT,  0,  STATS_NULL},
	{"  nice", HIDE_BIT,  0,  STATS_NULL},
	{"   sys", DETAIL_BIT,  0,  STATS_NULL},
	{"  idle", HIDE_BIT,  0,  STATS_NULL},
	{"  wait", HIDE_BIT,  0,  STATS_NULL},
	{"  hirq", HIDE_BIT,  0,  STATS_NULL},
	{"  sirq", HIDE_BIT,  0,  STATS_NULL},
	{" steal", HIDE_BIT,  0,  STATS_NULL},
	{" guest", HIDE_BIT,  0,  STATS_NULL},
        {"nr_throttled", DETAIL_BIT,  0,  STATS_NULL},
        {"throttled_time", DETAIL_BIT,  0,  STATS_NULL},
	/* mem info 16-50*/
	{"  cach", DETAIL_BIT,  0,  STATS_NULL},
	{"  used", DETAIL_BIT,  0,  STATS_NULL},
	{"mtotal", DETAIL_BIT,  0,  STATS_NULL},
	{"  free", DETAIL_BIT,  0,  STATS_NULL},
	{" mutil", SUMMARY_BIT,  0,  STATS_NULL},
	{"pgfault", DETAIL_BIT,  0,  STATS_NULL},
	{"pgmajfault", DETAIL_BIT,  0,  STATS_NULL},
	{"mfailcnt", DETAIL_BIT,  0,  STATS_NULL},
	{" drgl1", HIDE_BIT,  0,  STATS_NULL},
	{" drgl5", HIDE_BIT,  0,  STATS_NULL},
	{"drgl10", HIDE_BIT,  0,  STATS_NULL},
	{"drgl100", HIDE_BIT,  0,  STATS_NULL},
	{"drgl500", HIDE_BIT,  0,  STATS_NULL},
	{"drgl1s", HIDE_BIT,  0,  STATS_NULL},
	{"drgl1se", HIDE_BIT,  0,  STATS_NULL},
	{"drglcnt", HIDE_BIT,  0,  STATS_NULL},
	{"drgltime", HIDE_BIT,  0,  STATS_NULL},
	{" drml1", HIDE_BIT,  0,  STATS_NULL},
	{" drml5", HIDE_BIT,  0,  STATS_NULL},
	{"drml10", HIDE_BIT,  0,  STATS_NULL},
	{"drml100", HIDE_BIT,  0,  STATS_NULL},
	{"drml500", HIDE_BIT,  0,  STATS_NULL},
	{"drml1s", HIDE_BIT,  0,  STATS_NULL},
	{"drml1se", HIDE_BIT,  0,  STATS_NULL},
	{"drmlcnt", HIDE_BIT,  0,  STATS_NULL},
	{"drmltime", HIDE_BIT,  0,  STATS_NULL},
	{"  dcl1", HIDE_BIT,  0,  STATS_NULL},
	{"  dcl5", HIDE_BIT,  0,  STATS_NULL},
	{" dcl10", HIDE_BIT,  0,  STATS_NULL},
	{"dcl100", HIDE_BIT,  0,  STATS_NULL},
	{"dcl500", HIDE_BIT,  0,  STATS_NULL},
	{" dcl1s", HIDE_BIT,  0,  STATS_NULL},
	{"dcl1se", HIDE_BIT,  0,  STATS_NULL},
	{"dclcnt", HIDE_BIT,  0,  STATS_NULL},
	{"dcltime", HIDE_BIT,  0,  STATS_NULL},
	/* io info 51-64*/
	{" riops", DETAIL_BIT,  MERGE_SUM,  STATS_NULL},
	{" wiops", DETAIL_BIT,  MERGE_SUM,  STATS_NULL},
	{"  rbps", DETAIL_BIT,  MERGE_SUM,  STATS_NULL},
	{"  wbps", DETAIL_BIT,  MERGE_SUM,  STATS_NULL},
	{" rwait", HIDE_BIT,  MERGE_AVG,  STATS_NULL},
	{" wwait", HIDE_BIT,  MERGE_AVG,  STATS_NULL},
	{"  rsrv", HIDE_BIT,  MERGE_AVG,  STATS_NULL},
	{"  wsrv", HIDE_BIT,  MERGE_AVG,  STATS_NULL},
	{"  rioq", HIDE_BIT,  MERGE_AVG,  STATS_NULL},
	{"  wioq", HIDE_BIT,  MERGE_AVG,  STATS_NULL},
	{"rioqsz", HIDE_BIT,  MERGE_AVG,  STATS_NULL},
	{"wioqsz", HIDE_BIT,  MERGE_AVG,  STATS_NULL},
	{"rarqsz", DETAIL_BIT,  MERGE_AVG,  STATS_NULL},
	{"warqsz", DETAIL_BIT,  MERGE_AVG,  STATS_NULL},
	/* hw_events info 65-70 */
	{"cycle", HIDE_BIT,  0,  STATS_NULL},
	{"instr", HIDE_BIT,  0,  STATS_NULL},
	{"llcref", HIDE_BIT,  0,  STATS_NULL},
	{"llcmis", HIDE_BIT,  0,  STATS_NULL},
	{"CPI", HIDE_BIT,  0,  STATS_NULL},
	{"dltCPI", HIDE_BIT,  0,  STATS_NULL},
	/* jitter info 71-76 */
	{"numrsw", DETAIL_BIT,  0,  STATS_NULL},	/* total numbers of runqslow jitter */
	{" tmrsw", DETAIL_BIT,  0,  STATS_NULL},	/* the sum-time of runqslow delay */
	{"numnsc", DETAIL_BIT,  0,  STATS_NULL},	/* total numbers of nosched jitter */
	{" tmnsc", DETAIL_BIT,  0,  STATS_NULL},	/* the sum-time of nosched delay */
	{"numirq", DETAIL_BIT,  0,  STATS_NULL},	/* total numbers of irqoff jitter */
	{" tmirq", DETAIL_BIT,  0,  STATS_NULL},	/* the sum-time of irqoff delay */
};

#define NR_CGROUP_INFO sizeof(cg_info)/sizeof(struct mod_info)

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
 *@subpath: get the path the container's cgroup path
 *@pid: the Pid of the container
 *@cg: the group, like cpuset, cpu, memory...
 * */
static inline int get_con_cgpath_bypid(char *subpath, long pid, const char* cg, int alen)
{
	int ret = -1;
	FILE *fp;
	char cgroup_path[64] = {0};

	snprintf(cgroup_path, sizeof(cgroup_path), "/proc/%ld/cgroup", pid);
	fp = fopen(cgroup_path, "r");
	if (!fp)
		return errno;

	memset(buf_1024, 0, 1024);
	while(fgets(buf_1024, 1024, fp)) {
		size_t len;
		char *token;
		if((token = strstr(buf_1024, cg)) != NULL) {
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

static inline char *get_cgroup_path(const char *name, const char *cg, char *path, int len)
{
	int ret, index;
	long pid;

	pid = get_con_pid(name);
	if (pid <= 0)
		return NULL;
	index = snprintf(path, len, "/sys/fs/cgroup/%s/", cg);
	ret = get_con_cgpath_bypid(path+index, pid, cg, len-index-1);
	if (!ret)
		return path;

	return NULL;
}

static unsigned long cgroup_init_time;
static int need_reinit(void)
{
	if (cgroup_init_time <= (time(NULL) - CGROUP_INFO_AGING_TIME))
		return 1;

	return 0;
}

static int perf_event_init(struct cgroup_info *cgroup)
{
	int cgrp_id, ret = 0;
	int cpu, **fds;
	long long **counts;
	char path[LEN_256];

	if (!get_cgroup_path(cgroup->name, "perf_event", path, sizeof(path)))
		return -1;

	cgrp_id = open(path, O_RDONLY);
	if (cgrp_id < 0) {
		ret = errno;
		fprintf(stderr, "%s :open %s\n", strerror(errno), path);
		return ret;
	}

	nr_cpus = sysconf(_SC_NPROCESSORS_CONF);
	fds = malloc(nr_cpus * sizeof(int*));
	if (!fds) {
		fprintf(stderr, "%s :malloc fds fail\n", strerror(errno));
		return -ENOMEM;
	}
	counts = malloc(nr_cpus * sizeof(long long*));
	if (!counts) {
		fprintf(stderr, "%s :malloc counsts fail\n", strerror(errno));
		free(fds);
		return -ENOMEM;
	}
	for (cpu = 0; cpu < nr_cpus; cpu++) {
		long long *cnts;
		int i, *grp_fds, gleader_fd = -1;

		grp_fds =  malloc(NCONF_HW * sizeof(int));
		if (!grp_fds) {
			fprintf(stderr, "%s :malloc grp_fds for cpu%d\n", strerror(errno),cpu);
			fds[cpu] = NULL;
			continue;
		}
		fds[cpu] = grp_fds;
		cnts =  malloc(NCONF_HW * sizeof(long long));
		if (!cnts) {
			fprintf(stderr, "%s :malloc cnts for cpu%d\n", strerror(errno),cpu);
			counts[cpu] = NULL;
			free(fds[cpu]);
			continue;
		}
		counts[cpu] = cnts;
		struct perf_event_attr attr = {
			.freq = 0,
			.disabled = 1,
			.sample_period = 5*1000*1000-1,
		};
		for (i = 0; i < NCONF_HW; i++) {
			attr.type = hwtypes[i];
			attr.config = hwconfigs[i];
			fds[cpu][i] = syscall(__NR_perf_event_open, &attr, cgrp_id,
						cpu, gleader_fd, PERF_FLAG_PID_CGROUP);
			if (fds[cpu][i] < 0) {
				if (errno == ENODEV)
					continue;
				continue;
			}
			if (i == 0)
				gleader_fd = fds[cpu][i];
			counts[cpu][i] = 0;
		}
		ioctl(gleader_fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
		ioctl(gleader_fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
	}
	cgroup->hwres.hw_fds = fds;
	cgroup->hwres.hw_counters = counts;
	return ret;
}

static int check_and_init_shm(void)
{
	int fd, ret, i;
	char *p;
	static int jitshm_inited = 0;

	if (jitshm_inited) {
		return 0;
	}

	fd = shm_open(jit_shm_key, O_CREAT|O_RDWR|O_TRUNC, 06666);
	if (fd < 0) {
		ret = errno;
		perror("shm_open jit_shm_key");
		return ret;
	}

	ret = ftruncate(fd, sizeof(struct jitter_shm));
	if (ret < 0) {
		ret = errno;
		perror("ftruncate jit_shm_key");
		goto shm_fail;
	}

	p = mmap(NULL, sizeof(struct jitter_shm),
		PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (p == MAP_FAILED) {
		ret = errno;
		perror("mmap jit_shm_key");
		goto shm_fail;
	}

	jitshm_inited = 1;
	jitshm = (struct jitter_shm *)p;
	for (i = 0; i < CPU_ARRY_LEN; i++) {
		struct jit_lastN *lastiq, *lastnc, *lastrq = &jitshm->irqoff.lastN_array[i];
		lastiq = &jitshm->irqoff.lastN_array[i];
		lastnc = &jitshm->nosched.lastN_array[i];
		lastrq = &jitshm->rqslow.lastN_array[i];
		
		memset(lastiq->con, '0', sizeof(lastiq->con));
		memset(lastnc->con, '0', sizeof(lastnc->con));
		memset(lastrq->con, '0', sizeof(lastrq->con));
	}
	return 0;

shm_fail:
	shm_unlink(jit_shm_key);
	return ret;
}

static int cg_jitter_inited(void)
{
	int *jitter_symbol;
	int jit_inited;
	void *handle = dlopen(NULL, RTLD_LAZY);
	if (handle) {
		jitter_symbol = dlsym(handle, "jitter_init");
		if (jitter_symbol)
			jit_inited = *jitter_symbol + cg_jitter_init;
		else
			jit_inited = cg_jitter_init;
	} else {
		fprintf(stderr, "cgroup:dlopen NULL fail\n");
		jit_inited = -1;
	}
	return jit_inited;
}

static int cg_init_jitter(void)
{
	int ret;
	FILE *fp1, *fp2, *fp3;

	ret = check_and_init_shm();
	if (ret < 0)
		return ret;

	ret = cg_jitter_inited();
	if (ret > 0)
		return 0;
	else if (ret < 0)
		return ret;

	/* todo: what if command can't be find? */
	/* threshold is 40ms */
	fp1 = popen("sysak runqslower -S sysak_mservice_jitter_shm 40 2>/dev/null &", "r");
	if (!fp1) {
		perror("popen runqslower");
		return -1;
	}

	fp2 = popen("sysak nosched -S sysak_mservice_jitter_shm -t 10 2>/dev/null &", "r");
	if (!fp2) {
		perror("popen nosched");
		return -1;
	}

	fp3 = popen("sysak irqoff -S sysak_mservice_jitter_shm  10 2>/dev/null &", "r");
	if (!fp3) {
		perror("popen irqoff");
		return -1;
	}
	cg_jitter_init = 1;
	return 0;
}

int enum_containers(void)
{
	int i, perf_fail = 0;
	FILE *result;

	result = popen("docker ps -q --no-trunc", "r");
	memset(buffer, 0, sizeof(buffer));
	for (i = 0; i < MAX_CGROUPS && !feof(result); i++) {
		if (feof(result) || !fgets(buffer, sizeof(buffer), result))
			break;
		sscanf(buffer, "%65s", cgroups[n_cgs].name);
		if (perf_event_init(&cgroups[n_cgs]) < 0) {
			perf_fail++;
			fprintf(stderr, "%s:Perf init failed\n", cgroups[n_cgs].name);
		}
		n_cgs++;
	}
	if (!perf_fail) {
		for (i = 65; i < 71; i++)
			cg_info[i].summary_bit = DETAIL_BIT;
	}

	pclose(result);
	return 0;
}

bool is_docker_path(char *dentry)
{
	int i;

	if (64 != strlen(dentry))
		return false;

	for (i = 0; i < strlen(dentry); i++) {
		if ((dentry[i] >= '0' && dentry[i] <= '9')
		   ||(dentry[i] >= 'a' && dentry[i] <= 'f'))
			continue;
		return false;
	}
	return true;
}

int enum_containers_ext(char *parent)
{
	int ret;
	char path[256+64];
	int perf_fail = 0;
	DIR *d = NULL;
	struct dirent *entp = NULL;
	struct stat stats;

	if (stat(parent, &stats) < 0 || !S_ISDIR(stats.st_mode)) {
		ret = errno?errno:-1;
		fprintf(stderr, "stat %s:%s", parent, strerror(ret));
		return ret;
	}

	if(!(d = opendir(parent))) {
		ret = errno;
		fprintf(stderr, "opendir:%s", strerror(errno));
		return ret;
	}

	while((entp = readdir(d)) != NULL) {
		if((!strcmp(entp->d_name, ".")) || (!strcmp(entp->d_name, "..")))
			continue;
		snprintf(path, sizeof(path)-1, "%s/%s", parent, entp->d_name);
		if (stat(path, &stats) < 0) {
			fprintf(stderr, "stat %s:%s", path, strerror(errno));
			continue;
		}
		if (S_ISDIR(stats.st_mode) && is_docker_path(entp->d_name)) {
			sscanf(entp->d_name, "%65s", cgroups[n_cgs].name);
			if (perf_event_init(&cgroups[n_cgs]) < 0) {
				perf_fail++;
				fprintf(stderr, "%s:Perf init failed\n", cgroups[n_cgs].name);
			}
			n_cgs++;
			if (n_cgs >= MAX_CGROUPS)
				break;
		}
		if (!perf_fail) {
			int i;
			for (i = 65; i < 71; i++)
				cg_info[i].summary_bit = DETAIL_BIT;
		}
	}
	return 0;
}

bool dockerd_alive(void)
{
	bool ret = false;
	FILE *fp;
	char comm[16];
	char pid[16];
	char proc_path[32];
	char *dockerpid="/var/run/docker.pid";

	fp = fopen(dockerpid, "r");
	if (fp) {
		if (!fgets(pid, sizeof(pid), fp))
			goto out;
		fclose(fp);
		snprintf(proc_path, sizeof(proc_path), "/proc/%s/comm", pid);
		fp = fopen(proc_path, "r");
		if (fp) {
			if (!fgets(comm, sizeof(comm), fp))
				goto out;
			if (!strncmp(comm, "dockerd", strlen("dockerd"))) {
				ret =  true;
				goto out;
			}
		}
	}

	return ret;
out:
	fclose(fp);
	return ret;
}

static void init_cgroups(void)
{
	int i, ret = -1;

	if (n_cgs > 0 && !need_reinit())
		return;

	memset(cgroups, 0, sizeof(cgroups));
	n_cgs = 0;

	/*check docker exit*/
	if (access("/bin/docker", F_OK) != F_OK &&
		access("/usr/bin/docker", F_OK) != F_OK &&
		access("/bin/docker", F_OK) != F_OK &&
		access("/usr/bin/docker", F_OK) != F_OK) {
		ret = enum_containers_ext("/sys/fs/cgroup/cpu/docker/");
	} else {
		ret = enum_containers();
	}

	ret = cg_init_jitter();
	if (!ret) {
		for (i = 71; i < 76; i++)
			cg_info[i].summary_bit = DETAIL_BIT;
	}
	cgroup_init_time = time(NULL);
}

static int get_load_and_enhanced_cpu_stats(int cg_idx)
{
	char filepath[LEN_1024];
	FILE *file;
	int items = 0, ret = 0;

	if (!get_cgroup_path(cgroups[cg_idx].name, "cpuacct", filepath, sizeof(filepath)))
		return 0;

	strcat(filepath, "/cpuacct.proc_stat");
	file = fopen(filepath, "r");
	if (!file)
		return 0;

	while (fgets(buffer, sizeof(buffer), file)) {
		items += ret;
		if (items == 14)
			break;
		ret = sscanf(buffer, "user %llu", &cgroups[cg_idx].cpu.cpu_user);
		if (ret != 0) {
			continue;
		}
		ret = sscanf(buffer, "nice %llu", &cgroups[cg_idx].cpu.cpu_nice);
		if (ret != 0) {
			cg_info[6].summary_bit = DETAIL_BIT;
			continue;
		}
		ret = sscanf(buffer, "system %llu", &cgroups[cg_idx].cpu.cpu_sys);
		if (ret != 0) {
			continue;
		}
		ret = sscanf(buffer, "idle %llu", &cgroups[cg_idx].cpu.cpu_idle);
		if (ret != 0) {
			cg_info[8].summary_bit = DETAIL_BIT;
			continue;
		}
		ret = sscanf(buffer, "iowait %llu", &cgroups[cg_idx].cpu.cpu_iowait);
		if (ret != 0) {
			cg_info[9].summary_bit = DETAIL_BIT;
			continue;
		}
		ret = sscanf(buffer, "irq %llu", &cgroups[cg_idx].cpu.cpu_hirq);
		if (ret != 0) {
			cg_info[10].summary_bit = DETAIL_BIT;
			continue;
		}
		ret = sscanf(buffer, "softirq %llu", &cgroups[cg_idx].cpu.cpu_sirq);
		if (ret != 0) {
			cg_info[11].summary_bit = DETAIL_BIT;
			continue;
		}
		ret = sscanf(buffer, "steal %llu", &cgroups[cg_idx].cpu.cpu_steal);
		if (ret != 0) {
			cg_info[12].summary_bit = DETAIL_BIT;
			continue;
		}
		ret = sscanf(buffer, "guest %llu", &cgroups[cg_idx].cpu.cpu_guest);
		if (ret != 0) {
			continue;
		}
		ret = sscanf(buffer, "load average(1min) %d", &cgroups[cg_idx].load.load_avg_1);
		if (ret != 0) {
			cg_info[0].summary_bit = DETAIL_BIT;
			continue;
		}
		ret = sscanf(buffer, "load average(5min) %d", &cgroups[cg_idx].load.load_avg_5);
		if (ret != 0) {
			cg_info[1].summary_bit = DETAIL_BIT;
			continue;
		}
		ret = sscanf(buffer, "load average(15min) %d", &cgroups[cg_idx].load.load_avg_15);
		if (ret != 0) {
			cg_info[2].summary_bit = DETAIL_BIT;
			continue;
		}
		ret = sscanf(buffer, "nr_running %d", &cgroups[cg_idx].load.nr_running);
		if (ret != 0) {
			cg_info[3].summary_bit = DETAIL_BIT;
			continue;
		}
		ret = sscanf(buffer, "nr_uninterruptible %d", &cgroups[cg_idx].load.nr_uninterrupt);
		if (ret != 0) {
			cg_info[4].summary_bit = DETAIL_BIT;
			continue;
		}
	}

	fclose(file);
	return items;
}

static int get_cpu_stats(int cg_idx)
{
	char filepath[LEN_1024];
	FILE *file;
	int items = 0, ret = 0;

	if (!get_cgroup_path(cgroups[cg_idx].name, "cpu", filepath, sizeof(filepath)))
		return 0;

	strcat(filepath, "/cpu.stat");
	file = fopen(filepath, "r");
	if (!file)
		return 0;

	while (fgets(buffer, sizeof(buffer), file)) {
		items += ret;
		if (items == 2)
			break;
		ret = sscanf(buffer, "nr_throttled %llu", &cgroups[cg_idx].cpu.nr_throttled);
		if (ret != 0) {
			continue;
		}
		ret = sscanf(buffer, "throttled_time %llu", &cgroups[cg_idx].cpu.throttled_time);
		if (ret != 0) {
			continue;
		}
	}

	fclose(file);
	return items;
}

static int get_mem_latency(FILE *file, struct cg_memlat_info *info)
{
	int items = 0, ret = 0;

	info->total_lat_cnt = 0;
	while (fgets(buffer, sizeof(buffer), file)) {
		items += ret;
		ret = sscanf(buffer, "0-1ms: %llu", &info->lat_1);
		if (ret != 0) {
			info->total_lat_cnt += info->lat_1;
			continue;
		}
		ret = sscanf(buffer, "1-5ms: %llu", &info->lat_5);
		if (ret != 0) {
			info->total_lat_cnt += info->lat_5;
			continue;
		}
		ret = sscanf(buffer, "5-10ms: %llu", &info->lat_10);
		if (ret != 0) {
			info->total_lat_cnt += info->lat_10;
			continue;
		}
		ret = sscanf(buffer, "10-100ms: %llu", &info->lat_100);
		if (ret != 0) {
			info->total_lat_cnt += info->lat_100;
			continue;
		}
		ret = sscanf(buffer, "100-500ms: %llu", &info->lat_500);
		if (ret != 0) {
			info->total_lat_cnt += info->lat_500;
			continue;
		}
		ret = sscanf(buffer, "500-1000ms: %llu", &info->lat_1s);
		if (ret != 0) {
			info->total_lat_cnt += info->lat_1s;
			continue;
		}
		ret = sscanf(buffer, ">=1000ms: %llu", &info->lat_over1s);
		if (ret != 0) {
			info->total_lat_cnt += info->lat_over1s;
			continue;
		}

		ret = sscanf(buffer, "total(ms): %llu", &info->total_lat_time);
		if (ret != 0)
			continue;
	}

	return items;
}

static void set_mem_latency_visible(void)
{
	int i, offset = 24;/* Todo, the idx of memlat_info begin*/
	int cnt = sizeof(struct cg_memlat_info) / sizeof(unsigned long long) * 3;

	for(i = 0; i < cnt; i++)
		cg_info[i + offset].summary_bit = DETAIL_BIT;
}

static int get_memory_stats(int cg_idx)
{
	char filepath[LEN_1024];
	char *path_end = filepath;
	FILE *file;
	int items = 0, ret = 0;
	unsigned long long active_file, inactive_file, usage_in_bytes;

	if (!get_cgroup_path(cgroups[cg_idx].name, "memory", filepath, sizeof(filepath)))
		return 0;

	path_end = filepath + strlen(filepath);
	strcpy(path_end, "/memory.stat");
	file = fopen(filepath, "r");
	if (!file)
		return 0;

	while (fgets(buffer, sizeof(buffer), file)) {
		items += ret;
		if (items == 6)
			break;
		ret = sscanf(buffer, "cache %llu", &cgroups[cg_idx].mem.cache);
		if (ret != 0) {
			continue;
		}
		ret = sscanf(buffer, "pgfault %llu", &cgroups[cg_idx].mem.pgfault);
		if (ret != 0) {
			continue;
		}
		ret = sscanf(buffer, "pgmjfault %llu", &cgroups[cg_idx].mem.pgmjfault);
		if (ret != 0) {
			continue;
		}
		ret = sscanf(buffer, "inactive_file %llu", &inactive_file);
		if (ret != 0) {
			continue;
		}
		ret = sscanf(buffer, "active_file %llu", &active_file);
		if (ret != 0) {
			continue;
		}
		ret = sscanf(buffer, "hierarchical_memory_limit %llu", &cgroups[cg_idx].mem.limit);
		if (ret != 0) {
			continue;
		}
	}

	fclose(file);

	strcpy(path_end, "/memory.usage_in_bytes");
	file = fopen(filepath, "r");
	if (!file)
		return 0;

	fgets(buffer, sizeof(buffer), file);
	ret = sscanf(buffer, "%llu", &usage_in_bytes);
	fclose(file);
	if (ret <=0)
		return 0;
	cgroups[cg_idx].mem.used = usage_in_bytes - inactive_file - active_file;
	cgroups[cg_idx].mem.free = cgroups[cg_idx].mem.limit - usage_in_bytes;

	strcpy(path_end, "/memory.failcnt");
	file = fopen(filepath, "r");
	if (!file)
		return 0;
	fgets(buffer, sizeof(buffer), file);
	ret = sscanf(buffer, "%llu", &cgroups[cg_idx].mem.failcnt);
	fclose(file);
	if (ret <=0)
		return 0;

	strcpy(path_end, "/memory.direct_reclaim_global_latency");
	file = fopen(filepath, "r");
	if (!file)
		return 0;
	ret = get_mem_latency(file, &cgroups[cg_idx].mem.drgl);
	fclose(file);
	if (ret <=0)
		return 0;

	strcpy(path_end, "/memory.direct_reclaim_memcg_latency");
	file = fopen(filepath, "r");
	if (!file)
		return 0;
	ret = get_mem_latency(file, &cgroups[cg_idx].mem.drml);
	fclose(file);
	if (ret <=0)
		return 0;

	strcpy(path_end, "/memory.direct_compact_latency");
	file = fopen(filepath, "r");
	if (!file)
		return 0;
	ret = get_mem_latency(file, &cgroups[cg_idx].mem.dcl);
	fclose(file);
	if (ret <= 0)
		return 0;

	set_mem_latency_visible();
	return items;
}

static int read_io_stat(const char *filepath, unsigned long long *rval, unsigned long long *wval)
{
	FILE *file;
	char *pbuf;
	int items = 0, ret = 0;

	file = fopen(filepath, "r");
	if (!file)
		return 0;

	while (fgets(buffer, sizeof(buffer), file)) {
		items += ret;
		if (items == 2)
			break;
		pbuf = strstr(buffer, "Read");
		if (pbuf) {
			ret = sscanf(pbuf, "Read %llu", rval);
			if (ret != 1) {
				return 0;
			}
			continue;
		}

		pbuf = strstr(buffer, "Write");
		if (pbuf) {
			ret = sscanf(pbuf, "Write %llu", wval);
			if (ret != 1) {
				return 0;
			}
			continue;
		}
	}

	fclose(file);
	if (items != 2)
		return 0;

	return items;
}

static int get_blkinfo_stats(int cg_idx)
{
	char filepath[LEN_1024];
	char *path_end = filepath;
	int items = 0, ret = 0;

	if (!get_cgroup_path(cgroups[cg_idx].name, "blkio", filepath, sizeof(filepath)))
		return 0;

	path_end = filepath + strlen(filepath);
	strcpy(path_end, "/blkio.throttle.io_serviced");
	ret = read_io_stat(filepath, &cgroups[cg_idx].blkio.rd_ios, &cgroups[cg_idx].blkio.wr_ios);
	if (ret <=0)
		return 0;
	items += ret;

	strcpy(path_end, "/blkio.throttle.io_service_bytes");
	ret = read_io_stat(filepath, &cgroups[cg_idx].blkio.rd_bytes, &cgroups[cg_idx].blkio.wr_bytes);
	if (ret <=0)
		return 0;
	items += ret;

	strcpy(path_end, "/blkio.throttle.io_wait_time");
	ret = read_io_stat(filepath, &cgroups[cg_idx].blkio.rd_wait, &cgroups[cg_idx].blkio.wr_wait);
	if (ret <=0)
		return items;
	cg_info[55].summary_bit = DETAIL_BIT;
	cg_info[56].summary_bit = DETAIL_BIT;

	strcpy(path_end, "/blkio.throttle.io_service_time");
	ret = read_io_stat(filepath, &cgroups[cg_idx].blkio.rd_time, &cgroups[cg_idx].blkio.wr_time);
	if (ret <=0)
		return items;
	cg_info[57].summary_bit = DETAIL_BIT;
	cg_info[58].summary_bit = DETAIL_BIT;

	strcpy(path_end, "/blkio.throttle.total_io_queued");
	ret = read_io_stat(filepath, &cgroups[cg_idx].blkio.rd_qios, &cgroups[cg_idx].blkio.wr_qios);
	if (ret <=0)
		return items;
	cg_info[59].summary_bit = DETAIL_BIT;
	cg_info[60].summary_bit = DETAIL_BIT;

	strcpy(path_end, "/blkio.throttle.total_bytes_queued");
	ret = read_io_stat(filepath, &cgroups[cg_idx].blkio.rd_qbytes, &cgroups[cg_idx].blkio.wr_qbytes);
	if (ret <=0)
		return items;

	cg_info[61].summary_bit = DETAIL_BIT;
	cg_info[62].summary_bit = DETAIL_BIT;
	return items;
}

static int get_hwres_stats(int cg_idx)
{
	long long count, pre_cycle, pre_ins;
	int i, j, nr;
	struct cg_hwres_info *hwres;

	hwres = &cgroups[cg_idx].hwres;
	if (!hwres->hw_fds || !hwres->hw_counters)
		return 0;	//todo

	pre_cycle = hwres->hw_sum[0];
	pre_ins = hwres->hw_sum[1];
	for (i = 0; i < nr_cpus; i++) {
		if (!hwres->hw_fds[i] || !hwres->hw_counters[i])
			continue;
		for (j = 0; j < NCONF_HW; j++) {
			int fd = hwres->hw_fds[i][j];
			if (fd < 0)
				continue;
			nr = read(fd, &count, sizeof(long long));
			if (nr < 0)
				continue;
			hwres->hw_counters[i][j] = count;
			hwres->hw_sum[j] += count;
		}
	}
	if (hwres->hw_sum[1])
		hwres->CPI = (100*hwres->hw_sum[0])/hwres->hw_sum[1];
	else
		hwres->CPI = 0;
	if (hwres->hw_sum[1]-pre_ins)
		hwres->deltaCPI = (100*(hwres->hw_sum[0]-pre_cycle))/
				(hwres->hw_sum[1]-pre_ins);
	else
		hwres->deltaCPI = 0;
	return nr_cpus*sizeof(long long);
}

static inline unsigned long jitter_cgroup_matched(int cg_idx, struct jit_lastN *lasti)
{
	return (!strncmp(cgroups[cg_idx].name, lasti->con, CONID_LEN));
}

static int get_jitter_stats(int cg_idx)
{
	int i = 0, j, idx;
	struct cg_jitter_info *jits;
	struct jit_lastN *lasti;
	struct sched_jit_summary *jitsum[JITTER_NTYPE];

	jitsum[0] = &jitshm->nosched;
	jitsum[1] = &jitshm->irqoff;
	jitsum[2] = &jitshm->rqslow;
	jits = &cgroups[cg_idx].jitter;
	for (j = 0; j < JITTER_NTYPE; j++) {
		struct jitter_info *jit;

		jit = &jits->info[j];
		i = -1;
		while(i < JITITEM) {
			i++;
			lasti = &jitsum[j]->lastN_array[i];
			if (!jitter_cgroup_matched(cg_idx, lasti))
				continue;
			jit->idx = idx = (jit->idx+1)%JITITEM;
			jit->num = jit->num + 1;
			jit->time = jit->time + lasti->delay;
			jit->lastcpu[idx] = lasti->cpu;
		}
	}
	return 0;
}

void get_cgroup_stats(void)
{
	int i, items;

	n_cgs = (n_cgs>MAX_CGROUPS)?MAX_CGROUPS:n_cgs;
	for (i = 0; i < n_cgs; i++) {
		items = 0;
		items += get_load_and_enhanced_cpu_stats(i);
		items += get_cpu_stats(i);
		items += get_memory_stats(i);
		items += get_blkinfo_stats(i);
		items += get_hwres_stats(i);
		items += get_jitter_stats(i);
		cgroups[i].valid = !!items;
	}
}

static int print_cgroup_load(char *buf, int len, struct cg_load_info *info)
{
	return snprintf(buf, len, "%d,%d,%d,%d,%d,",
			info->load_avg_1,
			info->load_avg_5,
			info->load_avg_15,
			info->nr_running,
			info->nr_uninterrupt);
}

static int print_cgroup_cpu(char *buf, int len, struct cg_cpu_info *info)
{
	return snprintf(buf, len, "%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,",
			info->cpu_user,
			info->cpu_nice,
			info->cpu_sys,
			info->cpu_idle,
			info->cpu_iowait,
			info->cpu_hirq,
			info->cpu_sirq,
			info->cpu_steal,
			info->cpu_guest,
			info->nr_throttled,
			info->throttled_time);
}

static int print_memlat_info(char *buf, int len, struct cg_memlat_info *info)
{
	return snprintf(buf, len, "%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,",
			info->lat_1, info->lat_5, info->lat_10, info->lat_100,
			info->lat_500, info->lat_1s, info->lat_over1s,
			info->total_lat_cnt, info->total_lat_time);
}

static int print_cgroup_memory(char *buf, int len, struct cg_mem_info *info)
{
	int ret;

	ret = snprintf(buf, len, "%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,",
			info->cache, info->used, info->limit, info->free,
			info->util, info->pgfault, info->pgmjfault,
			info->failcnt);

	ret += print_memlat_info(buf + ret, len - ret, &info->drgl);
	ret += print_memlat_info(buf + ret, len - ret, &info->drml);
	ret += print_memlat_info(buf + ret, len - ret, &info->dcl);

	return ret;
}

static int print_cgroup_blkio(char *buf, int len, struct cg_blkio_info *info)
{
	return snprintf(buf, len, "%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,",
			info->rd_ios, info->wr_ios,
			info->rd_bytes, info->wr_bytes,
			info->rd_wait, info->wr_wait,
			info->rd_time, info->wr_time,
			info->rd_qios, info->wr_qios,
			info->rd_qbytes, info->wr_qbytes,0ULL,0ULL);
}

static int print_cgroup_hwres(char *buf, int len, struct cg_hwres_info *info)
{
	int i, pos = 0;

	for (i = 0; i < NCONF_HW; i++)
		pos += snprintf(buf + pos, len - pos, "%lld,", info->hw_sum[i]);
	pos += snprintf(buf + pos, len - pos, "%lld,", info->CPI);
	pos += snprintf(buf + pos, len - pos, "%lld,", info->deltaCPI);
	return pos;
}

static int print_cgroup_jitter(char *buf, int len, struct cg_jitter_info *infos)
{
	int i, pos = 0;
	struct jitter_info *info;

	for (i = 0; i <  JITTER_NTYPE - 1; i++) {
		info = &infos->info[i];
		pos += snprintf(buf + pos, len - pos,
			"%lu,%llu,", info->num, info->time);
	}
	info = &infos->info[i];
	pos += snprintf(buf + pos, len - pos,
		"%lu,%llu", info->num, info->time);
	return pos;
}

void
print_cgroup_stats(struct module *mod)
{
	int pos = 0, i;
	char buf[LEN_1M], name[13] = {0};

	memset(buf, 0, LEN_1M);

	n_cgs = (n_cgs>MAX_CGROUPS)?MAX_CGROUPS:n_cgs;
	for (i = 0; i < n_cgs; i++) {
		if (!cgroups[i].valid)
			continue;
		strncpy(name,cgroups[i].name, 12);
		pos += snprintf(buf + pos, LEN_1M - pos, "%s=", name);
		pos += print_cgroup_load(buf + pos, LEN_1M - pos, &cgroups[i].load);
		pos += print_cgroup_cpu(buf + pos, LEN_1M - pos, &cgroups[i].cpu);
		pos += print_cgroup_memory(buf + pos, LEN_1M - pos, &cgroups[i].mem);
		pos += print_cgroup_blkio(buf + pos, LEN_1M - pos, &cgroups[i].blkio);
		pos += print_cgroup_hwres(buf + pos, LEN_1M - pos, &cgroups[i].hwres);
		pos += print_cgroup_jitter(buf + pos, LEN_1M - pos, &cgroups[i].jitter);
		pos += snprintf(buf + pos, LEN_1M - pos, ITEM_SPLIT);
	}
	set_mod_record(mod, buf);
}

void
read_cgroup_stat(struct module *mod, char *parameter)
{
	init_cgroups();
	get_cgroup_stats();
	print_cgroup_stats(mod);
}

static void
set_load_record(double st_array[], U_64 cur_array[])
{
	int i;
	for (i = 0; i < 3; i++) {
		st_array[i] = cur_array[i] / 100.0;
	}
	st_array[3] = cur_array[3];
	st_array[4] = cur_array[4];
}

static void
set_cpu_record(double st_array[],
    U_64 pre_array[], U_64 cur_array[])
{
	int    i, j;
	U_64   pre_total, cur_total;

	pre_total = cur_total = 0;

	for (i = 0; i < 11; i++) {
		if(cur_array[i] < pre_array[i]){
			for(j = 0; j < 11; j++)
				st_array[j] = -1;
			return;
		}

		if (i < 9) {
			pre_total += pre_array[i];
			cur_total += cur_array[i];
		}
	}

	/* no tick changes, or tick overflows */
	if (cur_total <= pre_total) {
		for(j = 0; j < 9; j++)
			st_array[j] = -1;
		return;
	}

	/* set st record */
	for (i = 0; i < 9; i++) {
		st_array[i] = (cur_array[i] - pre_array[i]) * 100.0 / (cur_total - pre_total);
	}
	st_array[9] = (cur_array[9] - pre_array[9]);
	st_array[10] = (cur_array[10] - pre_array[10]);
}

static void
set_memory_record(double st_array[], U_64 pre_array[], U_64 cur_array[])
{
	int i;
	int lat_cnt = sizeof(struct cg_memlat_info) / sizeof(unsigned long long) * 3;

	for (i = 0; i < 4; i++) {
		st_array[i] = cur_array[i];
	}

	st_array[4] = cur_array[1] * 100.0 / cur_array[2];

	for (i = 5; i < (lat_cnt + 8); i++) {
		st_array[i] = cur_array[i] - pre_array[i];
	}
}

static void
set_blkio_record(double st_array[], U_64 pre_array[], U_64 cur_array[], int inter)
{
	int i, data_valid = 1;

	for(i = 0; i < 11; i++){
		if(cur_array[i] < pre_array[i]) {
			data_valid = 0;
			break;
		}
	}

	if (!data_valid) {
		for(i = 0; i < 14; i++)
			st_array[i] = -1;
		return;
	}

	unsigned long long rd_ios = cur_array[0] - pre_array[0];
	unsigned long long wr_ios = cur_array[1] - pre_array[1];
	unsigned long long rd_bytes = cur_array[2] - pre_array[2];
	unsigned long long wr_bytes = cur_array[3] - pre_array[3];
	unsigned long long rd_wait = cur_array[4] - pre_array[4];
	unsigned long long wr_wait = cur_array[5] - pre_array[5];
	unsigned long long rd_time = cur_array[6] - pre_array[6];
	unsigned long long wr_time = cur_array[7] - pre_array[7];
	unsigned long long rd_qios = cur_array[8] - pre_array[8];
	unsigned long long wr_qios = cur_array[9] - pre_array[9];
	unsigned long long rd_qbytes = cur_array[10] - pre_array[10];
	unsigned long long wr_qbytes = cur_array[11] - pre_array[11];

	st_array[0] = rd_ios / (inter * 1.0);
	st_array[1] = wr_ios / (inter * 1.0);
	st_array[2] = rd_bytes / (inter * 1.0);
	st_array[3] = wr_bytes / (inter * 1.0);
	st_array[8] = rd_qios / (inter * 1.0);
	st_array[9] = wr_qios / (inter * 1.0);
	st_array[10] = rd_qbytes / (inter * 1.0);
	st_array[11] = wr_qbytes / (inter * 1.0);

	if (rd_ios) {
		st_array[4] = rd_wait / (rd_ios * 1.0);
		st_array[6] = rd_time / (rd_ios * 1.0);
		st_array[12] = rd_bytes / (rd_ios * 1.0);
	} else {
		st_array[4] = 0;
		st_array[6] = 0;
		st_array[12] = 0;
	}

	if (wr_ios) {
		st_array[5] = wr_wait / (wr_ios * 1.0);
		st_array[7] = wr_time / (wr_ios * 1.0);
		st_array[13] = wr_bytes / (wr_ios * 1.0);
	} else {
		st_array[5] = 0;
		st_array[7] = 0;
		st_array[13] = 0;
	}
}

static void
set_hwres_record(double st_array[], U_64 pre_array[], U_64 cur_array[])
{
	/*
 	 * cpu_cycles  :        65
 	 * instructions:        66
 	 * llc_R_acce  :        67
 	 * llc_R_miss  :        68
 	 * CPI         :        69
 	 * delta CPI   :        70
 	* */
	st_array[0] = cur_array[0];
	st_array[1] = cur_array[1];
	st_array[2] = cur_array[2];
	st_array[3] = cur_array[3];
	if (pre_array[0])
		st_array[4] = (100*cur_array[0])/cur_array[1];
	else
		st_array[4] = 0;
	if (cur_array[0]-pre_array[0] == 0)
		st_array[5] = 0;
	else
		st_array[5] = (100*(cur_array[0]-pre_array[0]))/
			(cur_array[1]-pre_array[1]);
	pre_array[0] = cur_array[0];
	pre_array[1] = cur_array[1];
}

static void
set_cg_jit_record(double st_array[], U_64 pre_array[], U_64 cur_array[])
{
	/*
 	 * numrqs: numbers of runqueue-slower jitter	71
 	 * tmrqs:  time of runqeueue-slower jitter	72
 	 * numnsc: numbers of system-delay jitter	73
 	 * tmnsc:  time of system-delay jitter		74
 	 * numirq: numbers of irqoff of jitter		75
 	 * tmirq:  time of irqoff jitter		76
 	* */
	st_array[0] = cur_array[0] - pre_array[0];
	st_array[1] = cur_array[1] - pre_array[1];
	st_array[2] = cur_array[2] - pre_array[2];
	st_array[3] = cur_array[3] - pre_array[3];
	st_array[4] = cur_array[4] - pre_array[4];
	st_array[5] = cur_array[5] - pre_array[5];
}
static void
set_cgroup_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
	set_load_record(st_array, cur_array);
	set_cpu_record(&st_array[5], &pre_array[5], &cur_array[5]);
	set_memory_record(&st_array[16], &pre_array[16], &cur_array[16]);
	set_blkio_record(&st_array[51], &pre_array[51], &cur_array[51], inter);
	set_hwres_record(&st_array[65], &pre_array[65], &cur_array[65]);
	set_cg_jit_record(&st_array[71], &pre_array[71], &cur_array[71]);
}

char *cg_lable = "cgroup";

void
mod_register(struct module *mod)
{
	mod->lable = cg_lable;
    register_mod_fields(mod, "--cg", cg_usage, cg_info, NR_CGROUP_INFO, read_cgroup_stat, set_cgroup_record);
}
