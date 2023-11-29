#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <fcntl.h>

#include "iosdiag.h"

static void usage(void)
{
	fprintf(stdout,
		"\nUsage: \n"
		"latency [OPTION] disk_devname       Detect IO latency in specified disk\n"
		"latency -t ms disk_devname          Set IO latency threshold(default 1000ms)\n"
		"latency -T sec disk_devname         How long to detect IO latency(default 10s)\n"
		"latency -a sec disk_devname         The period of time in the detach state(default 0s)\n"
		"latency -f log disk_devname         Specify the output file log\n"
		"latency -m                          Monitor mode, sending high-latency IO events to unity\n"
		"latency -v                          Display debug log during load bpf\n"
		"\ne.g.\n"
		"latency vda                         Detect IO latency in disk \"vda\"\n"
		"latency -t 10 vda                   Set IO latency threshold 10ms and detect IO latency in disk \"vda\"\n"
		"latency -t 10 -T 30 vda             Detect IO latency in disk \"vda\" 30 secs\n"
		"latency -t 10 -T 30 -m              Detect IO latency in all disks 30 secs and send high-latency IO events to unity\n");

	exit(-1);
}

static int g_verbose;
int enable_debug_log(void)
{
	return g_verbose;
}

static unsigned long g_threshold_us;
unsigned long get_threshold_us(void)
{
	return g_threshold_us;
}

int check_bpf_config() {
    char command[200];
    char config[200];
    char line[200];
    FILE *fp;

    strcpy(command, "grep \"CONFIG_BPF=y\" /boot/config-");
    strcpy(config, command); 
    strcat(command, "`uname -r`");

    fp = popen(command, "r");
    if (fp == NULL) {
        printf("check bpf config command error\n");
        return -1; 
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strlen(line) > 0) {
            printf("found CONFIG_BPF=y\n");
            pclose(fp);
            return 1; 
        }
    }

    printf("CONFIG_BPF=y not found\n");
    pclose(fp);
    return 0;
}

int main(int argc, char *argv[])
{
	int ch;
	int timeout_s = 10, threshold_ms = 1000;
	unsigned int attach_s = 0;
	int operating_mode = DIAGNOSTIC_MODE;
	char *result_dir = "/var/log/sysak/iosdiag/latency";
	char *tool_path = "/usr/local/sysak/.sysak_components/tools";
	char *devname;
	char resultfile_path[256];

	while ((ch = getopt(argc, argv, "a:mT:t:f:hv")) != -1) {
		switch (ch) {
			case 'a':
				attach_s = (unsigned int)strtoul(optarg, NULL, 0);
				if (attach_s < 0)
					attach_s = 0;
				break;		
			case 'm':
				operating_mode = MONITOR_MODE;
				break;
			case 'T':
				timeout_s = (unsigned int)strtoul(optarg, NULL, 0);
				if (timeout_s <= 0)
					timeout_s = 10;
				break;
			case 't':
				threshold_ms = (int)strtoul(optarg, NULL, 0);
				break;
			case 'f':
				result_dir = optarg;
				break;
			case 'v':
				g_verbose = 1;
				break;
			case 'h':
			default:
				usage();
		}
	}

	devname = argv[argc - 1];
	g_threshold_us = threshold_ms * 1000;
	sprintf(resultfile_path, "%s/result.log.seq", result_dir);
	
	if (!check_bpf_config()) {
    	char command[100];
		char execute_path[100];
		sprintf(execute_path, "%s/tracing_latency", tool_path);
		strcpy(command, execute_path);

		char arg1[10];
		sprintf(arg1, "%d", threshold_ms); 
		strcat(command, " -t ");
		strcat(command, arg1);

		char arg2[10];
		sprintf(arg2, "%d", timeout_s); 
		strcat(command, " -T ");
		strcat(command, arg2);

		if (devname != NULL) {
			strcat(command, " -d ");
			strcat(command, devname);
		}

		strcat(command, " -f ");
		strcat(command, resultfile_path);

    	int status = 0;
    	status = system(command); 

    	if (status == -1) {
    	    printf("Error executing script\n");
    	    return 1;
    	} else {
    	    return 0;
    	}
	} else{
		if (iosdiag_init(devname, attach_s)) {
			fprintf(stderr, "iosdiag_init fail\n");
			return -1;
		}
		iosdiag_run(timeout_s, operating_mode, resultfile_path);
		iosdiag_exit(devname);
		return 0;
	}
}

