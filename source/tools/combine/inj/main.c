#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

typedef int (*INJ_FUNC)(void *args);
struct inj_op {
	const char *inj_name;
	INJ_FUNC inject;
	INJ_FUNC check_result;
};

int inject_oops(void *args);
int inject_panic(void *args);
int inject_hung_task_panic(void *args);
int inject_high_load(void *args);
int inject_high_sys(void *args);
int inject_softlockup(void *args);
int inject_taskhang(void *args);
int inject_taskloop(void *args);
int inject_tasklimit(void *args);
int inject_fdlimit(void *args);
int inject_oom(void *args);
int inject_packdrop(void *args);
int inject_runqlat(void *args);
int inject_iolat(void *args);
int inject_netlat(void *args);

int check_high_load(void *args);
int check_high_sys(void *args);
int check_softlockup(void *args);
int check_taskhang(void *args);
int check_taskloop(void *args);
int check_tasklimit(void *args);
int check_fdlimit(void *args);
int check_oom(void *args);
int check_packdrop(void *args);
int check_runqlat(void *args);
int check_iolat(void *args);
int check_netlat(void *args);

struct inj_op inj_ops[] = {
	{"oops", inject_oops, NULL},
	{"panic", inject_panic, NULL},
	{"hung_task_panic", inject_hung_task_panic, NULL},
	{"high_load", inject_high_load, check_high_load},
	{"high_sys", inject_high_sys, check_high_sys},
	{"softlockup", inject_softlockup, check_softlockup},
	{"taskhang", inject_taskhang, check_taskhang},
	{"taskloop", inject_taskloop, check_taskloop},
	{"tasklimit", inject_tasklimit, check_tasklimit},
	{"fdlimit", inject_fdlimit, check_fdlimit},
	{"oom", inject_oom, check_oom},
	{"packdrop", inject_packdrop, check_packdrop},
	{"runqlat", inject_runqlat, check_runqlat},
	{"iolat", inject_iolat, check_iolat},
	{"netlat", inject_netlat, check_netlat},
};

#define NUM_INJ_OPS (sizeof(inj_ops)/sizeof(struct inj_op))

int inject_oops(void *args)
{
	return 0;
}

int inject_panic(void *args)
{
        return 0;
}

int inject_hung_task_panic(void *args)
{
        return 0;
}

int inject_high_load(void *args)
{
        return 0;
}

int inject_high_sys(void *args)
{
        return 0;
}

int inject_softlockup(void *args)
{
        return 0;
}

int inject_taskhang(void *args)
{
        return 0;
}

int inject_taskloop(void *args)
{
        return 0;
}

int inject_tasklimit(void *args)
{
        return 0;
}

int inject_fdlimit(void *args)
{
        return 0;
}

int inject_oom(void *args)
{
        return 0;
}

int inject_packdrop(void *args)
{
        return 0;
}

int inject_runqlat(void *args)
{
        return 0;
}

int inject_iolat(void *args)
{
        return 0;
}

int inject_netlat(void *args)
{
        return 0;
}

int check_high_load(void *args)
{
        return 0;
}

int check_high_sys(void *args)
{
        return 0;
}

int check_softlockup(void *args)
{
        return 0;
}

int check_taskhang(void *args)
{
        return 0;
}

int check_taskloop(void *args)
{
        return 0;
}

int check_tasklimit(void *args)
{
        return 0;
}

int check_fdlimit(void *args)
{
        return 0;
}

int check_oom(void *args)
{
        return 0;
}

int check_packdrop(void *args)
{
        return 0;
}

int check_runqlat(void *args)
{
        return 0;
}

int check_iolat(void *args)
{
        return 0;
}

int check_netlat(void *args)
{
        return 0;
}

int main(int argc, char *argv[])
{
	int ret = -EINVAL, i;
	const char *args = NULL;

	if (argc < 2)
		return ret;

	if (argc == 3)
		args = argv[2];

	for (i = 0; i < NUM_INJ_OPS; i++) {
		if (!strcmp(argv[1], inj_ops[i].inj_name)) {
			ret = inj_ops[i].inject(args);
			if (ret == 0){
				if (inj_ops[i].check_result)
					ret = inj_ops[i].check_result(args);
			}
 
			break;
		}
	}

	if (ret)
		printf("Failed\n");
	else
		printf("Done\n");

	return 0;
}

