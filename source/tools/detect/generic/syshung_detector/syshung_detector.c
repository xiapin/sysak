#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <getopt.h>
#include <time.h>
#include <dirent.h>
#include "cJSON.h"
#include "syshung_detector.h"


char load_1[MAX_INDEX_LEN];

double load_avg_1;     /* 1 min average load */
long smp_num_cpus;     /* number of CPUs */

bool is_json = FALSE;
bool use_bpf = FALSE;
bool sys_check = FALSE;
bool collect_data = FALSE;
bool has_task = FALSE;
unsigned long pid = 0;
struct syshung_info g_syshung;
char syshung_sysdata_subdir[MAX_PATH_LEN];
char syshung_sysdata_bpf[MAX_PATH_LEN];

struct service_info service_arry[MAX_NR_SERVICES];

struct option longopts[] = {
    { "json", no_argument, NULL, 'j' },
    { "data", no_argument, NULL, 'd' },
    { "check", no_argument, NULL, 'c' },
    { "bpf", no_argument, NULL, 'b' },
    { "pid", no_argument, NULL, 'p' },
    { "help", no_argument, NULL, 'h' },
    { 0, 0, 0, 0},
};

static void usage(void)
{
	fprintf(stdout,
	            "Usage: syshung_detector [options]\n"
                "Options:\n"
                "    -c     check system hung\n"
                "    -j     date display esult by json\n"
                "    -d     collect system data without bpf tools\n"
                "    -b     collect system data with bpf tools\n"
                "    -p     collect specify task info\n"
                "    -h     help\n");
    exit(0);
}

static void json_print(void)
{
	int i;
	cJSON *root;
	char *out;
	cJSON *next;

    root = cJSON_CreateObject();
    //next = cJSON_CreateObject();
	//cJSON_AddItemToObject(root, "syshung result", next);
    
    cJSON_AddStringToObject(root, "isHung", g_syshung.is_hung? "yes":"no");
	cJSON_AddStringToObject(root, "hungClass", fault_class_name[g_syshung.hung_class]);
    cJSON_AddStringToObject(root, "hungEvent", hung_event[g_syshung.event]);
    cJSON_AddNumberToObject(root, "load_1", g_syshung.load_1);
    cJSON_AddNumberToObject(root, "dztaskCounts", g_syshung.dz_counts);
    cJSON_AddNumberToObject(root, "sysUtils", g_syshung.sys);
    cJSON_AddNumberToObject(root, "memUtils", g_syshung.mem);
    cJSON_AddStringToObject(root, "expService", g_syshung.name);

	out = cJSON_Print(root);
    printf("%s\n", out);
    free(out);
    //cJSON_Delete(next);
    cJSON_Delete(root);
}

static bool log_detect(char *path)
{
    FILE *fp;
    char buf[MAX_BUFF_LEN];
    int ret = FALSE;


    if (access(path,0) != 0)
        return ret;

    fp = fopen(path, "r");
    if (!fp){
        printf("open %s failed\n", path);
		return ret;
    }
    while(fgets(buf, sizeof(buf), fp))
    {
        //printf("buff is :%s\n",buf);
        if (strstr(buf,"soft lockup")){
            g_syshung.hung_class = FATAL_FAULT;
            g_syshung.event = HU_SOFTLOCKUP;
            ret = TRUE;
            goto out;
        }
        if (strstr(buf,"hard lockup") || strstr(buf,"hard LOCKUP")){
            g_syshung.hung_class = FATAL_FAULT;
            g_syshung.event = HU_HARDLOCKUP;
            ret = TRUE;
            goto out;
        }
        if (strstr(buf,"blocked for more than")){
            g_syshung.hung_class = NORMAL_FAULT;
            g_syshung.event = HU_HUNGTASK;
            ret = TRUE;
            goto out;
        }
        if (strstr(buf,"rcu_sched detected stalls")){
            g_syshung.hung_class = NORMAL_FAULT;
            g_syshung.event = HU_RCUSTALL;
            ret = TRUE;
            goto out;
        }
    }
out:
    fclose(fp);
    return ret;
}

void get_nr_cpu(void)
{
   smp_num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (smp_num_cpus<1)
        smp_num_cpus=1;
}

static bool load_detect(char *path)
{
    FILE *fp;
    char buf[MAX_BUFF_LEN];

    memset(load_1,0,MAX_INDEX_LEN);


    if (access(path,0) != 0)
        return FALSE;

    fp = fopen(path, "r");
    if (!fp){
        printf("open %s failed\n", path);
        fclose(fp);
		return FALSE;
    }

    while(fgets(buf, sizeof(buf), fp))
    {
        sscanf(buf,"%[^ ]",load_1);
    }
    g_syshung.load_1 = strtod(load_1, NULL);

    get_nr_cpu();
    //printf("LOAD_CPUS_SCALE * smp_num_cpus is %d\n",(LOAD_CPUS_SCALE * smp_num_cpus) / 2);
    if ((long)load_avg_1 >= (LOAD_CPUS_SCALE * smp_num_cpus) / 2){
        g_syshung.hung_class = NORMAL_FAULT;
        g_syshung.event = HU_HIGHLOAD;
        return TRUE;
    }
    fclose(fp);
    return FALSE;
}

static int calc_taskcount(char *path,int *count)
{
    FILE *fp;
    char buf[MAX_BUFF_LEN];

    if (access(path,0) != 0)
        return FALSE;
    fp = fopen(path, "r");

    while(fgets(buf, sizeof(buf), fp))
    {
        if (strstr(buf,"Name:"))
            *count++;
    }

}

static bool dztask_detect(void)
{
    FILE *fp;
    char buf[MAX_BUFF_LEN];
    char dtask_file[MAX_PATH_LEN];
    char rtask_file[MAX_PATH_LEN];
    char ztask_file[MAX_PATH_LEN];
    bool ret = FALSE;
    int rtasks_count = 0;
    int dtasks_count = 0;
    int ztasks_count = 0;

    fp = popen("sysak loadtask -s -f  2>/dev/null &", "r");
	if (!fp) {
		perror("popen loadtask\n");
		return ret;
	}
    pclose(fp);

    snprintf(rtask_file, sizeof(rtask_file), "%s%s", 
        loadtask_log_dir, rtask_filename);
    snprintf(dtask_file, sizeof(dtask_file), "%s%s", 
        loadtask_log_dir, dtask_filename);
    snprintf(ztask_file, sizeof(ztask_file), "%s%s", 
        loadtask_log_dir, ztask_filename);

    calc_taskcount(rtask_file,&rtasks_count);
    calc_taskcount(dtask_file,&dtasks_count);
    calc_taskcount(ztask_file,&ztasks_count);

    g_syshung.dz_counts = dtasks_count + ztasks_count;

    if (dtasks_count >= MAX_D_TASKS || ztasks_count >= MAX_D_TASKS){
        g_syshung.hung_class = NORMAL_FAULT;
        g_syshung.event = HU_DZTASK;
        ret = TRUE;
    }
    return ret;
}

static double get_utils(char *m_arg, char *log, char *pattern)
{
    FILE *fp;
    char *ptr;
    int ret = FALSE;
    double utils_v = 0;
    char buf[MAX_BUFF_LEN];
    char utils[MAX_INDEX_LEN];
    char mservice_info[MAX_PATH_LEN];
    char syshung_info[MAX_PATH_LEN];

    if (access(syshung_log_dir,0) != 0)
        mkdir(syshung_log_dir, 0755 );
    
    snprintf(mservice_info, sizeof(mservice_info), "%s %s > %s%s", 
        mservice_cmd, m_arg, syshung_log_dir, log);
    
    fp = popen(mservice_info, "r");
	if (!fp) {
		printf("popen memservice %s failed\n",m_arg);
		return ret;
	}
    pclose(fp); 
    snprintf(syshung_info, sizeof(syshung_info), "%s%s", syshung_log_dir, log);

    fp = fopen(syshung_info, "r");
    if (!fp){
        printf("open %s failed\n", syshung_info);
		return ret;
    }

    while(fgets(buf, sizeof(buf), fp))
    {
        if(ptr = strstr(buf,pattern)){
            sscanf(ptr + strlen(pattern),"%[^ ]", utils);
        }
    }
    utils_v = strtod(utils,NULL);
    return utils_v;
}

static bool system_detect(void)
{
    int ret = FALSE;
    double sys, mem;   

    if (access(syshung_log_dir,0) != 0)
        mkdir(syshung_log_dir, 0755 );

    /* read cpu utils */
    //sys = get_utils(mservice_cpu_arg, syshung_cpu_log,"%*[^:]:%*[^:]:sys=%[^ ]%*");
    sys = get_utils(mservice_cpu_arg, syshung_cpu_log,"cpu:sys=");
    /* read mem utils */
    mem = get_utils(mservice_mem_arg, syshung_mem_log,"mem:util=");

    if ((long)sys > MAX_SYS){
        g_syshung.hung_class = SLIGHT_FAULT;
        g_syshung.event = HU_HIGHSYS;
        ret = TRUE;
    }

    if ((long)mem > MAX_MEM){
        g_syshung.hung_class = SLIGHT_FAULT;
        g_syshung.event = HU_HIGHMEM;
        ret = TRUE;
    }
    g_syshung.sys = sys;
    g_syshung.mem = mem;
    return ret;
}

static void str_strip(char *dst, char *str)
{
    char *tmp = NULL;
    if ((tmp = strstr(dst, str)))
        *tmp = '\0';
}

static void get_task_info(struct service_info *tsk_info)
{
    FILE *fp;
    char filename[MAX_PATH_LEN];
    char filename_rss[MAX_PATH_LEN], filename_cpu[MAX_PATH_LEN];
    char line[MAX_PATH_LEN];
    char pidof[MAX_PATH_LEN];
    char utils[MAX_INDEX_LEN];

    tsk_info->pid = 0;
    tsk_info->rss = 0;
    tsk_info->cpu = 0;

    snprintf(pidof, sizeof(pidof), "pidof %s", tsk_info->name);
    if ((fp = popen(pidof, "r")) == NULL){
		printf("opne %s failed\n",pidof);
		return;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        sscanf(line, "%u", &tsk_info->pid);
    }

    if (tsk_info->pid){
        snprintf(filename_rss, sizeof(filename_rss), PID_RSS, tsk_info->pid);
        snprintf(filename_cpu, sizeof(filename_cpu), PID_CPU, tsk_info->pid);
    
        if ((fp = popen(filename_rss, "r")) == NULL){
            printf("opne %s failed\n",filename_rss);
            return;
        }
        while (fgets(line, sizeof(line), fp) != NULL) {
            str_strip(line,"\n");
            tsk_info->rss = strtoull(line,NULL,10);
        }

        if ((fp = popen(filename_cpu, "r")) == NULL){
            printf("opne %s failed\n",filename_cpu);
            return;
        }
        while (fgets(line, sizeof(line), fp) != NULL) {
            str_strip(line,"\n");
            tsk_info->cpu = strtod(line,NULL);
        }
    }
}

static bool services_detect(void)
{
    int i = 0;

    for(i; i < MAX_NR_SERVICES; i++){
        strncpy(service_arry[i].name, service_name[i], sizeof(service_arry[i].name));
        get_task_info(&service_arry[i]);
        if (service_arry[i].rss >= MAX_MEM_SERVICES || service_arry[i].cpu >= MAX_CPU_SERVICES){
            g_syshung.hung_class = SLIGHT_FAULT;
            g_syshung.event = HU_SERVICES_EXP;
            strncpy(g_syshung.name, service_arry[i].name, sizeof(g_syshung.name));
        }
    }

}

static void hung_check(void)
{
    g_syshung.event = 0;
    g_syshung.hung_class = 0;

    /* fatal event check */
    log_detect(kern_log);

    /* normal event check */
    load_detect(kern_loadavg);
    dztask_detect();

    /* slight event check */
    system_detect();
    services_detect();

    if (!g_syshung.hung_class)
        g_syshung.is_hung = 0;
    else
        g_syshung.is_hung = 1;

    if (is_json)
        json_print();
    else{
        printf("isHung:%s\n", g_syshung.is_hung? "yes" : "no");
        printf("hungClass:%s\n", fault_class_name[g_syshung.hung_class]);
        printf("hungEvent:%s\n", hung_event[g_syshung.event]);
        printf("load_1:%f\n", g_syshung.load_1);
        printf("dztaskCounts:%d\n", g_syshung.dz_counts);
        printf("sysUtils:%f\n", g_syshung.sys);
        printf("memUtils:%f\n", g_syshung.mem);
        printf("expService:%s\n", g_syshung.name);
    }

}

static void get_time(char *date, int len)
{
    time_t tmpcal_t;
	struct tm *tmp_ptr = NULL;

	time(&tmpcal_t);
	tmp_ptr = localtime(&tmpcal_t);
    snprintf(date, len, "%d-%d-%d-%d-%d-%d", 1900+tmp_ptr->tm_year, 1+tmp_ptr->tm_mon,
            tmp_ptr->tm_mday, tmp_ptr->tm_hour, tmp_ptr->tm_min, tmp_ptr->tm_sec);
}

static void print_split(FILE *fp)
{
    fprintf(fp,"%s\n","--------------split line-----------------");
}

static int data_storage(char *src, char *dst)
{
    FILE *fp_src, *fp_dst;
    char buf[MAX_BUFF_LEN];

    if (access(src,0) != 0)
        return -1;

    fp_src = fopen(src, "r");
    if (!fp_src){
        printf("open %s failed\n", src);
		return -1;
    }

    fp_dst = fopen(dst, "a+");
    if (!fp_dst){
        printf("open %s failed\n", dst);
		return -1;
    }

    fprintf(fp_dst,"%s\n",src);
    while(fgets(buf, sizeof(buf), fp_src))
    {
        fprintf(fp_dst,"%s\n",buf);
    }
    print_split(fp_dst);
    fclose(fp_src);
    fclose(fp_dst);
    return 0;
}

static int cmd_exec(char *cmd_str, char *data_file)
{
    char exec_cmd[MAX_CMD_LEN];
    FILE *fp;

    fp = fopen(data_file, "a+");
    if (!fp){
        printf("open %s failed\n", data_file);
		return -1;
    }
    fprintf(fp,"%s\n",cmd_str);
    fclose(fp);

    snprintf(exec_cmd, sizeof(exec_cmd), "%s >> %s", cmd_str, data_file);
    system(exec_cmd);

    fp = fopen(data_file, "a+");
    if (!fp){
        printf("open %s failed\n", data_file);
		return -1;
    }
    print_split(fp);
    fclose(fp);
    return 0;
}

static int loop_dir(char *dir, char *file_name, char * storage)
{
    DIR *dp;
    int len;
    struct dirent *dirp;
    char full_path[MAX_PATH_LEN];

    if (!(dp = opendir(dir))) {
		return -1;
	}

	while ((dirp = readdir(dp)) != NULL) {
		len = strlen(dirp->d_name);
        if (!strcmp(dirp->d_name, ".") || !strcmp(dirp->d_name, ".."))
            continue;
        snprintf(full_path, sizeof(full_path), "cat %s/%s/%s", dir, dirp->d_name, file_name);
        cmd_exec(full_path, storage);

	}
}

static int dtask_stack(char *storage_file)
{
    FILE *fp;
    char loadtask_result[MAX_PATH_LEN];

    fp = popen("sysak loadtask -s >> /dev/null &", "r");
	if (!fp) {
		perror("popen loadtask\n");
		return -1;
	}
    pclose(fp);

    if (!access("/var/log/sysak/loadtask/.tmplog",0)){
        snprintf(loadtask_result, sizeof(loadtask_result), "cat %s", "/var/log/sysak/loadtask/.tmplog");
        cmd_exec(loadtask_result, storage_file);
    }
}

static int mem_data(void)
{
    char storage_file[MAX_PATH_LEN];
    char date[MAX_DATE_LEN];

    get_time(date, MAX_DATE_LEN);
    snprintf(storage_file, sizeof(storage_file), "%s/%s-%s", syshung_sysdata_subdir, syshung_mem_data,date);

    data_storage("/proc/meminfo",storage_file);
    data_storage("/proc/pagetypeinfo",storage_file);
    data_storage("/proc/buddyinfo",storage_file);
    data_storage("/proc/slabinfo",storage_file);
    data_storage("/sys/kernel/debug/dma_buf/bufinfo",storage_file);
    data_storage("/proc/vmstat",storage_file);
    sleep(5);
    data_storage("/proc/vmstat",storage_file);
    data_storage("/proc/cgroups",storage_file);

    data_storage("/sys/fs/cgroup/memory/memory.direct_compact_latency",storage_file);
    data_storage("/sys/fs/cgroup/memory/memory.direct_reclaim_global_latency",storage_file);
    data_storage("/sys/fs/cgroup/memory/memory.direct_reclaim_memcg_latency",storage_file);
    data_storage("/sys/fs/cgroup/memory/memory.direct_compact_latency",storage_file);

    cmd_exec("numastat",storage_file);
    cmd_exec("numastat -m",storage_file);
    cmd_exec("numactl --hardware",storage_file);
    printf("mem data collect succeeded!\n");
    return 0;
}

static int sched_data(void)
{
    char storage_file[MAX_PATH_LEN];
    char date[MAX_DATE_LEN];
    char sourcefile_softirq[MAX_PATH_LEN];
    char resultfile_softirq[MAX_PATH_LEN];
    char cmd_softirq[MAX_CMD_LEN];

    get_time(date, MAX_DATE_LEN);
    snprintf(storage_file, sizeof(storage_file), "%s/%s-%s", syshung_sysdata_subdir, syshung_sched_data,date);

    data_storage("/proc/stat", storage_file);
    data_storage("/proc/schedstat", storage_file);
    data_storage("/proc/sched_debug", storage_file);

    cmd_exec("sysak cpuirq -t -i 1",storage_file);
    snprintf(sourcefile_softirq, sizeof(sourcefile_softirq), "%s/%s-%s",
            syshung_sysdata_subdir, "sourcefile_softirq",date);
    snprintf(resultfile_softirq, sizeof(resultfile_softirq), "%s/%s-%s",
            syshung_sysdata_subdir, "resultfile_softirq",date);
    
    snprintf(cmd_softirq, sizeof(cmd_softirq), "sysak softirq -s %s;sleep 1;sysak softirq -s %s -r %s;cat %s",
            sourcefile_softirq, sourcefile_softirq, resultfile_softirq, resultfile_softirq);
    cmd_exec(cmd_softirq, storage_file);
    if (!has_task)
        dtask_stack(storage_file);
    printf("sched data collect succeeded!\n");
    return 0;
}

static int io_data(void)
{
    char storage_file[MAX_PATH_LEN];
    char date[MAX_DATE_LEN];

    get_time(date, MAX_DATE_LEN);
    snprintf(storage_file, sizeof(storage_file), "%s/%s-%s", syshung_sysdata_subdir, syshung_io_data, date);

    data_storage("/proc/diskstats",storage_file);
    data_storage("/etc/mtab",storage_file);
    cmd_exec("iotop -d 1 -n 1", storage_file);

    if (!access("/sbin/iotop",0))   
        cmd_exec("iostat", storage_file);
    loop_dir("/sys/class/scsi_host", "host_busy", storage_file);
    printf("io data collect succeeded!\n");
    return 0;
}

static int net_data(void)
{
    char storage_file[MAX_PATH_LEN];
    char date[MAX_DATE_LEN];

    get_time(date, MAX_DATE_LEN);
    snprintf(storage_file, sizeof(storage_file), "%s/%s-%s", syshung_sysdata_subdir, syshung_net_data, date);

    data_storage("/proc/net/dev",storage_file);
    data_storage("/proc/net/snmp",storage_file);
    data_storage("/proc/net/tcp",storage_file);
    data_storage("/proc/net/netstat",storage_file);
    printf("net data collect succeeded!\n");
    return 0;
}

void task_data_storage(char *f_name, unsigned long pid, char *storage)
{
    char task_file[MAX_PATH_LEN];

    snprintf(task_file, sizeof(task_file), "/proc/%ld/%s", pid, f_name);
    if (!access(task_file,0))
        data_storage(task_file,storage);
}

static int task_data(unsigned long id)
{
    char storage_file[MAX_PATH_LEN];
    char date[MAX_DATE_LEN];

    get_time(date, MAX_DATE_LEN);
    snprintf(storage_file, sizeof(storage_file), "%s/task-%ld-%s", syshung_sysdata_subdir, id, date);

    task_data_storage("comm",id,storage_file);   
    task_data_storage("status",id,storage_file);
    task_data_storage("stack",id,storage_file);
    task_data_storage("cgroup",id,storage_file);
    task_data_storage("stat",id,storage_file);
    task_data_storage("statm",id,storage_file);
    task_data_storage("syscall",id,storage_file);
    task_data_storage("io",id,storage_file);
    task_data_storage("sched",id,storage_file);
    task_data_storage("schedstat",id,storage_file);
    task_data_storage("maps",id,storage_file);
    task_data_storage("smaps",id,storage_file);
    task_data_storage("numa_maps",id,storage_file);
    return 0;
}



static void hung_data(void)
{
    char date[MAX_DATE_LEN];

    if (access(syshung_log_dir,0) != 0)
        mkdir(syshung_log_dir, 0755);

    if (access(syshung_sysdata_dir,0) != 0)
        mkdir(syshung_sysdata_dir, 0755);
    
    get_time(date, MAX_DATE_LEN);
    snprintf(syshung_sysdata_subdir, sizeof(syshung_sysdata_subdir), "%s/%s", syshung_sysdata_dir,date);

    if (access(syshung_sysdata_subdir,0) != 0)
        mkdir(syshung_sysdata_subdir, 0755);

    mem_data();
    sched_data();
    io_data();
    net_data();
}

static void hung_data_bpf(void)
{
    char date[MAX_DATE_LEN];
    char cmd_runqslower[MAX_CMD_LEN];
    char cmd_nosched[MAX_CMD_LEN];

    if (access(syshung_log_dir,0) != 0)
        mkdir(syshung_log_dir, 0755);

    if (access(syshung_sysdata_dir,0) != 0)
        mkdir(syshung_sysdata_dir, 0755);
    
    get_time(date, MAX_DATE_LEN);
    snprintf(syshung_sysdata_bpf, sizeof(syshung_sysdata_bpf), "%s/bpf-%s", syshung_sysdata_dir, date);

    cmd_exec("sysak reclaimhung -t 10", syshung_sysdata_bpf);
    cmd_exec("sysak worklatency -t 10", syshung_sysdata_bpf);
    cmd_exec("sysak iosdiag latency -t 1 -T 10 vda", syshung_sysdata_bpf);
    cmd_exec("sysak reclaimhung -t 10", syshung_sysdata_bpf);

    snprintf(cmd_runqslower, sizeof(cmd_runqslower), "sysak runqslower -f %s/%s -s 10 20;cat %s/%s",
            syshung_sysdata_dir, "runqslower_res", syshung_sysdata_dir, "runqslower_res");
    cmd_exec(cmd_runqslower, syshung_sysdata_bpf);

    snprintf(cmd_nosched, sizeof(cmd_nosched), "sysak nosched  -f %s/%s -s 20;cat %s/%s",
            syshung_sysdata_dir, "nosched_res", syshung_sysdata_dir, "nosched_res");
    cmd_exec(cmd_nosched, syshung_sysdata_bpf);
}

int main(int argc, char *argv[])
{
    int opt;

    while ((opt = getopt(argc, argv, "jhcdbp:")) != -1) {
        switch (opt) {
            case 'j':
                is_json = TRUE; 
                break;
            case 'p':
                if (optarg){
                    pid = (int)strtoul(optarg, NULL, 10);
                    has_task = TRUE;
                }
            case 'd':
                collect_data = TRUE; 
                break;
            case 'b':
                use_bpf = TRUE; 
                break;
            case 'c':
                sys_check = TRUE; 
                break;
            case 'h':
                usage();
                break;
            default:
                printf("must have parameter\n");
                usage();
                break;
        }
    }

    if (sys_check)
        hung_check();

    if (collect_data && !use_bpf)
        hung_data();

    if (use_bpf)
        hung_data_bpf();

    if (has_task)
        task_data(pid);

    return 0;
}