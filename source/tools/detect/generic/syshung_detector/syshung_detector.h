#ifndef _SYSHUNG_DETECTOR_H
#define _SYSHUNG_DETECTOR_H

#define bool int
#define TRUE 1
#define FALSE 0

#define MAX_INDEX_LEN 32
#define MAX_PATH_LEN 256
#define MAX_DATE_LEN 256
#define MAX_CMD_LEN 1024
#define MAX_BUFF_LEN 1024

#define MAX_D_TASKS 5
#define MAX_Z_TASKS 5
#define LOAD_CPUS_SCALE 3    /* div to 2 */
#define MAX_SYS 50
#define MAX_MEM 90

#define MAX_NR_SERVICES 5
#define MAX_NAME_SERVICES 64
#define MAX_CPU_SERVICES 60
#define MAX_MEM_SERVICES 3*1024*1024  /* KB */

#define PID_RSS	"cat /proc/%u/status  |grep VmRSS |awk -F \" \" '{print$2}'"
#define PID_CPU	"pidstat -p %u |tail -n 1 | awk -F \" \" '{print$8}'"

char *kern_log = "/var/log/kern";
char *kern_loadavg = "/proc/loadavg";
char *loadtask_log_dir = "/var/log/sysak/loadtask/";
char *syshung_log_dir = "/var/log/sysak/syshung_detector/";
char *syshung_sysdata_dir = "/var/log/sysak/syshung_detector/sysdata";
char *syshung_mem_data = "mem_data";
char *syshung_sched_data = "sched_data";
char *syshung_io_data = "io_data";
char *syshung_net_data = "net_data";

char *syshung_cpu_log = "syshung_cpuinfo";
char *syshung_mem_log = "syshung_meminfo";
char *rtask_filename = "runtask";
char *dtask_filename = "dtask";
char *ztask_filename = "ztask";

char *mservice_cmd = "sysak mservice";
char *mservice_cpu_arg = "--check --cpu";
char *mservice_mem_arg = "--check --mem";
char *event;

static char *service_name[] = {
    "systemd",
    "syslog-ng",
    "dockerd",
    "systemd-network",
    "dbus"
};

struct service_info {
    double cpu;
    unsigned long long rss;
    unsigned long long pid;
    char name[MAX_NAME_SERVICES];
};

struct syshung_info {
    int is_hung;
    int hung_class;
    int event;
    int dz_counts;
    double load_1;
    double sys;
    double mem;
    char name[MAX_NAME_SERVICES];
};

struct tasks_count {
    int r_tasks;
    int d_tasks;
    int z_tasks;
};

enum FAULT_CLASS {
    NO_FAULT,
	SLIGHT_FAULT,
	NORMAL_FAULT,
	FATAL_FAULT,
	FAULT_CLASSS_MAX
};

static char *fault_class_name[FAULT_CLASSS_MAX] = {
    "null",
	"Slight",
	"Normal",
	"Fatal"
};

char *hung_event[] = {
    "null",
    "hung_softlockup",
    "hung_hardlockup",
    "hung_hungtask",
    "hung_rcustall",
    "hung_highload",
    "hung_dztask",
    "hung_highsys",
    "hung_highmem",
    "hung_services_exp"
};
enum HUNG_EVENT {
	/*kernel fault events*/
    NO_EVENT,
    HU_SOFTLOCKUP,
    HU_HARDLOCKUP,
    HU_HUNGTASK,
    HU_RCUSTALL,
    HU_HIGHLOAD,
    HU_DZTASK,
    HU_HIGHSYS,
    HU_HIGHMEM,
    HU_SERVICES_EXP,
    HU_MAX
};
#endif
