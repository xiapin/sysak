#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "proc_loadavg.h"

#define LOADAVG_PATH	"/proc/loadavg"

struct stats_load {
	unsigned long nr_running;
	float  load_avg_1;
	float  load_avg_5;
	float  load_avg_15;
	unsigned int  nr_threads;
};

int init(void * arg)
{
	printf("proc_loadavg plugin install.\n");
	return 0;
}

int full_line(struct unity_line *uline)
{
	int ret;
	FILE *fp;
	char line[128];
	struct stats_load st_load;

	fp = NULL;
	errno = 0;
	if ((fp = fopen(LOADAVG_PATH, "r")) == NULL) {
		ret = errno;
		printf("WARN: proc_loadavg install FAIL fopen\n");
		return ret;
	}

	if (fscanf(fp, "%f %f %f %ld/%d %*d\n",
			&st_load.load_avg_1,
			&st_load.load_avg_5,
			&st_load.load_avg_15,
			&st_load.nr_running,
			&st_load.nr_threads) != 5) {
		fclose(fp);
		return;
	}
#ifdef DEBUG_LOADAVG
	printf("load1=%5f load5=%5f load15=%5f\n", st_load.load_avg_1, st_load.load_avg_5, st_load.load_avg_15);
#endif
	if (st_load.nr_running) {
		/* Do not take current process into account */
		st_load.nr_running--;
	}
	//unity_set_index(uline1[cpu], 0, "load", cpu_name);
	unity_set_value(uline, 0, "load1", st_load.load_avg_1*100);
	unity_set_value(uline, 1, "load5", st_load.load_avg_5*100);
	unity_set_value(uline, 2, "load15", st_load.load_avg_15*100);
	unity_set_value(uline, 3, "runq", st_load.nr_running);
	unity_set_value(uline, 4, "plit", st_load.nr_threads);

	if (fp)
		fclose(fp);
}

int call(int t, struct unity_lines* lines) {
	struct unity_line* line;

	unity_alloc_lines(lines, 1);
	line = unity_get_line(lines, 0);
	unity_set_table(line, "proc_loadavg");
	full_line(line);
	return 0;
}

void deinit(void)
{
	printf("proc_loadavg plugin uninstall\n");
}
