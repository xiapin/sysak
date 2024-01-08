#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/sysinfo.h>
#include <pthread.h>
#include <signal.h>
#include "memleak.h"
#include "user_api.h"

#define SLABINFO_FILE "/proc/slabinfo"
#define SLABINFO_MAX_LINES 100
#define SLABINFO_MAX_LINE_LENGTH 256

extern int read_meminfo(struct meminfo *mem);
extern int slab_main(struct memleak_settings *set, int fd);
extern int vmalloc_main(int argc, char **argv);
extern int page_main(struct memleak_settings *set, int fd);
static int error = 0;
static int off = 0;
static struct meminfo mem;

static void show_usage(void)
{
	printf("-t \n");
	printf("  slab: trace slab  leak\n");
	printf("  page: trace alloc page  leak\n");
    printf("  vmalloc: trace vmalloc  leak, must use root \n");
	printf("-i: trace internal,default 300s \n");
	printf("-s: stacktrace for memory alloc \n");
	printf("-d: memleak off \n");
	printf("-c: only check memleak,don't diagnose \n");
	printf("-n: trace slab name, defualt select the max size or objects \n");

}
static int check_memleak(int total, int used)
{
    return !!((used >=6*1024*1024)||((used > total*0.1)&&(used > 1024*1024*1.5)));
}
static int memleak_check_only(struct meminfo *mi)
{
    int ret = 0;
    int vmalloc = 0;

    read_meminfo(mi);
    vmalloc = vmalloc_main(0, NULL);
    printf("allocPages:%dM, uslab:%dM vmalloc:%dM\n", (mi->kernel)/1024, mi->uslabkb/1024, vmalloc/1024);
    printf("诊断结论:");
    if (mi->kernel < vmalloc)
        mi->kernel = vmalloc + 1;
    ret = mi->kernel - vmalloc;
    if (check_memleak(mi->tlmkb, ret)) {
        printf("alloc page memleak\n");
        return MEMLEAK_TYPE_PAGE;
    } else if (check_memleak(mi->tlmkb, mi->uslabkb)) {
        printf("slab memleak\n");
        return MEMLEAK_TYPE_SLAB;
    } else if (check_memleak(mi->tlmkb, vmalloc)) {
        printf("vmalloc memleak\n");
        return MEMLEAK_TYPE_VMALLOC;
    } else {
        printf(" no memleak\n");
    }
    return 0;
}

int validate_monitor_time (    char * optarg) {
	int monitor_time = 300;
	int rc = 1;

	if (optarg == NULL) {
		printf("Arguments needed in \"-i\".\n");
		rc = 0;
	} else {
		if (strchr(optarg, '.') == NULL &&
			sscanf(optarg, "%d", &monitor_time) &&
			monitor_time > 0) {
		} else {
			printf("Only the integer bigger than 0 is valid.\n");
			rc = 0;
		}
	}
	return rc;
}

int check_sys_kmalloc_list ( char * results[], char * kmalloc_name ) {
	int rc = 0;
	int count = 0;
	char slabinfo_line[SLABINFO_MAX_LINE_LENGTH];

	FILE* file = fopen(SLABINFO_FILE, "r");
	if (file == NULL) {
		printf("Fail to open \"/proc/slabinfo\".\n");
		return rc;
	}
	while (fgets(slabinfo_line, sizeof(slabinfo_line), file) != NULL) {
		char * token = strtok(slabinfo_line, " ");
		if (token != NULL && strncmp(token, "kmalloc", 7) == 0) {
			if (strcmp(token, kmalloc_name) == 0) {
				rc = 1;
				break;
			}
			results[count++] = strdup(token);
		}
	}
	fclose(file);

	if (!rc) {
		printf("You've probably entered the wrong name of kmalloc.\n");
		printf("The list of system-supported kmallocs: \n");
		for (int i = 0; i < count; i++) {
			printf("%s\n", results[i]);
			free(results[i]);
		}
	}

	return rc;
}

int validate_slab_name ( char * optarg ) {
	int rc;
	char * kmalloc_list[SLABINFO_MAX_LINES];
	char * kmalloc_name = "";

	if (optarg == NULL) {
		printf("Arguments needed in \"-n\".\n");
	} else {
		kmalloc_name = optarg;
	}
	rc = check_sys_kmalloc_list(kmalloc_list, kmalloc_name);

	return rc;
}



int get_arg(struct memleak_settings *set, int argc, char * argv[])
{
    int ch;

	while ((ch = getopt(argc, argv, "dshci:r:n:t:")) != -1)
	{
		switch (ch)
        {
			case 't':
				if (!strncmp("slab", optarg, 4))
					set->type = MEMLEAK_TYPE_SLAB;
				else if (!strncmp("page", optarg, 4))
					set->type = MEMLEAK_TYPE_PAGE;
				else if (!strncmp("vmalloc", optarg, 7))
					set->type = MEMLEAK_TYPE_VMALLOC;
                break;
			case 'i':
				if (validate_monitor_time(optarg))
					set->monitor_time = atoi(optarg);
				else
					error = 1;
                break;
			case 'r':
				set->rate = atoi(optarg);
                break;
            case 'c':
                memleak_check_only(&mem);
                error = 1;
                break;
			case 'n':
				if (validate_slab_name(optarg))
					strncpy(set->name, optarg, NAME_LEN - 1);
				else
					error = 1;
                break;
			case 'h':
				show_usage();
				error = 1;
				break;
			case 's':
				set->ext = 1;
				break;
			case 'd':
				off = 1;
				break;
			case '?':
                printf("Unknown option: %c\n",(char)optopt);
				error = 1;
                break;
		}
	}
}


static int memleak_open(void)
{
    int fd = 0;
    fd = open("/dev/sysak", O_RDWR);
    if (fd < 0) {
        printf("open memleak check error\n");
    }
   return fd; 
}

static int memleak_close(int fd)
{
    if (fd >0)
        close(fd);
}

static int memleak_off(void)
{
	int fd = 0;
    
    fd = memleak_open();
	ioctl(fd, MEMLEAK_OFF);
    memleak_close(fd);	
    return 0;
}

int main(int argc, char **argv)
{
	struct memleak_settings set;
	int ret = 0;
    int fd = 0;
	memset(&set, 0, sizeof(set));

	get_arg(&set, argc, argv);

	if (error)
		return 0;
	if (off) {
		memleak_off();
		printf("memleak off success\n");
		return 0;
	}
	printf("type %d\n", set.type);
    fd = memleak_open();
    if (fd < 0)
        return 0;
	switch (set.type) {

		case MEMLEAK_TYPE_VMALLOC:
			vmalloc_main(argc, argv);
			break;

		case MEMLEAK_TYPE_PAGE:
			page_main(&set, fd);
			break;

		default:
			slab_main(&set, fd);
			break;
	};
    memleak_close(fd);
	return 0;
}
