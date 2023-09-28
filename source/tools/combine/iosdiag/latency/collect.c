#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <linux/bpf.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <time.h>
#include <libgen.h>
#include <pthread.h>
#include "ebpf_load.h"
#include "iosdiag.h"
#include "cnf_put.h"
#include "aggregator.h"
#include "format_json.h"
#include <linux/version.h>

#define min(x, y)		((x) > (y) ? (y) : (x))

#define DECLEAR_BPF_OBJ(name)					\
	static struct name##_bpf *name;				\
	static int name##_bpf_load;				\

DECLEAR_BPF_OBJ(iosdiag_virtblk);
DECLEAR_BPF_OBJ(iosdiag_nvme);
DECLEAR_BPF_OBJ(iosdiag_scsi);
DECLEAR_BPF_OBJ(iosdiag_scsi_mq);
static int iosdiag_map;
static int iosdiag_virtblk_map;
static int iosdiag_maps_targetdevt;
static int iosdiag_maps_notify;
static int g_stop;
static int g_log_fd = -1;
static char *g_json_buf;

int metrics_num = 0;
int metrics_capacity = 10; 
char** metrics_array;
pthread_mutex_t metrics_mutex;

int req_array_length = 0;
int req_capacity = 10;
struct iosdiag_req* req_array;
pthread_mutex_t req_mutex;

int exit_flag = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
void signal_handler(int signum) 
{
    printf("Received signal %d\n", signum);

    pthread_mutex_lock(&mutex);
    exit_flag = 1;
    pthread_mutex_unlock(&mutex);

    pthread_cond_broadcast(&cond);

    printf("Waiting for all threads to exit...\n");
    printf("Exiting...\n");
    exit(0);
}

extern int enable_debug_log(void);
extern unsigned long get_threshold_us(void);
static int exec_shell_cmd(char *cmd)
{
	char buf[64];
	FILE *fp;

	if (!cmd)
		return -1;

	if ((fp = popen(cmd, "r")) == NULL) {
		fprintf(stderr, "exec \'%s\' fail\n", cmd);
		return -1;
	}

	while (fgets(buf, sizeof(buf) - 1, fp));
	pclose(fp);
	return 0;
}

static int over_threshold(struct iosdiag_req *iop)
{
	unsigned long threshold_ns = get_threshold_us() * 1000;
	unsigned long delay_ns = iop->ts[MAX_POINT-1] -
				iop->ts[IO_START_POINT];

	if (delay_ns >= threshold_ns)
		return 1;
	return 0;
}

static void iosdiag_store_result(void *ctx, int cpu, void *data, __u32 size)
{
	struct iosdiag_req *iop = (struct iosdiag_req *)data;
	char *buf = g_json_buf;
	int fd = g_log_fd;

	if (over_threshold(iop)) {
		set_check_time_date();
		summary_convert_to_json(buf, iop);
		//point_convert_to_json(buf + strlen(buf), iop);
		delay_convert_to_json(buf + strlen(buf), iop);
		write(fd, buf, strlen(buf));
	}
}

void event_aggregator() 
{
	pthread_mutex_lock(&req_mutex);
	req_array = (struct iosdiag_req*)malloc(req_capacity * sizeof(struct iosdiag_req));
	pthread_mutex_unlock(&req_mutex);
  	while (1) {
		pthread_mutex_lock(&mutex);
        if (exit_flag) {
            pthread_mutex_unlock(&mutex);
            break;
        }
        pthread_mutex_unlock(&mutex);

		int lockResult = pthread_mutex_lock(&req_mutex);
		//printf("req_array_length: %d\n", req_array_length);
        if (req_array_length > 1) {
      		int i, j;
      		for (i = 0; i < req_array_length; i++) {
				if (strlen(req_array[i].diskname) == 0 || 
				strlen(req_array[i].comm) == 0 || 
				strlen(req_array[i].op) == 0) {
					//printf("jump req_array.diskname: %d\n", i);
                	continue; 
            	}
        		int m = 0;
        		char *maxdelay_component = get_max_delay_component(&req_array[i]);
				//printf("maxdelay_component: %s\n", maxdelay_component);
				unsigned int sum_data_len = req_array[i].data_len;
        		unsigned long sum_max_delay = get_max_delay(&req_array[i]);
				unsigned long max_delay = sum_max_delay;
        		unsigned long sum_total_delay = get_total_delay(&req_array[i]);
				unsigned long max_total_delay = sum_total_delay;
				unsigned int max_total_dalay_idx = i;
				char* max_total_delay_diskname = req_array[i].diskname;
    			unsigned long* sum_component_delay = (unsigned long*)malloc(5 * sizeof(unsigned long));
				memset(sum_component_delay, 0, 5 * sizeof(unsigned long));
    			unsigned long* max_component_delay = (unsigned long*)malloc(5 * sizeof(unsigned long));
        		memset(max_component_delay, 0, 5 * sizeof(unsigned long));
				int count = 1;
				// printf("sum_max_delay: %lu\n", sum_max_delay);
				update_component_delay(&req_array[i], sum_component_delay, max_component_delay, 1);

				//printf("block: %lu, driver: %lu, disk: %lu, complete: %lu, done: %lu\n", sum_block, sum_driver, sum_disk, sum_complete, sum_done);

				for (j = i + 1; j < req_array_length; j++) {
                	if (check_aggregation_conditions(&req_array[i], &req_array[j])){
						//printf("all the same req_array.diskname: %s\n", req_array[j].diskname);
                    	count++;
						unsigned long aggregated_total_delay = get_total_delay(&req_array[j]);
						unsigned long aggregated_max_delay = get_max_delay(&req_array[j]);
                    	sum_total_delay += aggregated_total_delay;
						sum_max_delay += aggregated_max_delay;
            			sum_data_len += req_array[j].data_len;
						int flag = 0;
						if (max_total_delay < aggregated_total_delay) {
							flag = 1;
						}
						if (flag) {
							max_delay = aggregated_max_delay;
							max_total_delay = aggregated_total_delay;
							strcpy(max_total_delay_diskname, req_array[j].diskname); 
							// printf("find larger total_delay: %s\n", max_total_delay_diskname);
							max_total_dalay_idx = j;
							update_component_delay(&req_array[j], sum_component_delay, max_component_delay, 1);
						}else {
							update_component_delay(&req_array[j], sum_component_delay, max_component_delay, 0);
						}
						//printf("put %d into blank\n", j);
						memset(req_array[j].diskname, '\0', sizeof(req_array[j].diskname));
            		}
          		}
				printf("sum_block: %lu, sum_driver: %lu, sum_disk: %lu, sum_complete: %lu, sum_done: %lu\n", max_component_delay[0], max_component_delay[1], max_component_delay[2], max_component_delay[3], max_component_delay[4]);
				printf("aggregator count: %d\n", count);
        		sum_data_len /= count;
      			sum_max_delay /= count;
          		sum_total_delay /= count;
				for (m = 0; m < MAX_POINT - 1; m++) {
      				sum_component_delay[m] /= count;
				}

				set_check_time_date();
				char *latency_summary = malloc(JSON_BUFFER_SIZE);
				memset(latency_summary, 0x0, JSON_BUFFER_SIZE);

				pthread_mutex_lock(&metrics_mutex);
				if (metrics_num >= metrics_capacity) {
        			metrics_capacity *= 2; 
        			metrics_array = realloc(metrics_array, metrics_capacity * sizeof(char*));
    			}

				metrics_array[metrics_num] = malloc(JSON_BUFFER_SIZE);
				sprintf(latency_summary, 
					"sysom_iolatency,diskname=%s,"
					"comm=%s,"
					"iotype=%s,"
					"maxdelay_component=%s "
					"max_delay=%lu,"
					"total_delay=%lu,"
					"pid=%d,"
					"datalen=%d,"
					"queue_id=%d,"
					"initiated_cpu=%d,"
					"issue_cpu=%d,"
					"respond_cpu=%d,"
					"soft_interrupt_cpu=%d,"
					"block=%lu,"
					"driver=%lu,"
					"disk=%lu,"
					"complete=%lu,"
					"done=%lu",
					req_array[i].diskname, 
					req_array[i].comm, 
					req_array[i].op, 
					maxdelay_component, 
					sum_max_delay, 
					sum_total_delay, 
					req_array[i].pid, 
					sum_data_len,
					req_array[i].queue_id,
					req_array[i].cpu[0],
					req_array[i].cpu[1],
					req_array[i].cpu[2],
					req_array[i].cpu[3],
					sum_component_delay[0],
					sum_component_delay[1],
					sum_component_delay[2],
					sum_component_delay[3],
					sum_component_delay[4]
				);
				sprintf(latency_summary + strlen(latency_summary), "%s", "\n");
				free(max_component_delay);
				free(sum_component_delay);
				//printf("collected step1: %s\n", latency_summary);
				//printf("max_total_delay_diskname: %s\n", max_total_delay_diskname);

				sprintf(latency_summary + strlen(latency_summary), 
					"sysom_iolatency_max,diskname=%s,"
					"comm=%s,"
					"iotype=%s,"
					"maxdelay_component=%s "
					"max_delay=%lu,"
					"total_delay=%lu,"
					"pid=%d,"
					"datalen=%d,"
					"queue_id=%d,"
					"initiated_cpu=%d,"
					"issue_cpu=%d,"
					"respond_cpu=%d,"
					"soft_interrupt_cpu=%d,"
					"block=%lu,"
					"driver=%lu,"
					"disk=%lu,"
					"complete=%lu,"
					"done=%lu",
					max_total_delay_diskname, 
					req_array[max_total_dalay_idx].comm, 
					req_array[max_total_dalay_idx].op, 
					maxdelay_component, 
					max_delay, 
					max_total_delay, 
					req_array[max_total_dalay_idx].pid, 
					req_array[max_total_dalay_idx].data_len,
					req_array[max_total_dalay_idx].queue_id,
					req_array[max_total_dalay_idx].cpu[0],
					req_array[max_total_dalay_idx].cpu[1],
					req_array[max_total_dalay_idx].cpu[2],
					req_array[max_total_dalay_idx].cpu[3],
					max_component_delay[0],
					max_component_delay[1],
					max_component_delay[2],
					max_component_delay[3],
					max_component_delay[4]
				);

				// printf("collected aggregator latency: %s\n", latency_summary);
				strcpy(metrics_array[metrics_num], latency_summary);
				metrics_num++;
				pthread_mutex_unlock(&metrics_mutex);
				free(latency_summary);
        	} 
			req_array_length = 0;
			req_capacity = 10;
			req_array = (struct iosdiag_req*)realloc(req_array, req_capacity * sizeof(struct iosdiag_req));
			pthread_mutex_unlock(&req_mutex);
        } else if (req_array_length == 1){
			set_check_time_date();
			char *latency_summary = malloc(JSON_BUFFER_SIZE);
			memset(latency_summary, 0x0, JSON_BUFFER_SIZE);
			summary_convert_to_unity(latency_summary, &req_array[0]);
			req_array_length = 0;
			req_capacity = 10;
			req_array = (struct iosdiag_req*)realloc(req_array, req_capacity * sizeof(struct iosdiag_req));
			pthread_mutex_unlock(&req_mutex);

			pthread_mutex_lock(&metrics_mutex);
			if (metrics_num >= metrics_capacity) {
        		metrics_capacity *= 2; 
        		metrics_array = realloc(metrics_array, metrics_capacity * sizeof(char*));
    		}
		
			metrics_array[metrics_num] = malloc(JSON_BUFFER_SIZE);
			strcpy(metrics_array[metrics_num], latency_summary);
			metrics_num++;
			printf("collected 1: %s\n", latency_summary);

			pthread_mutex_unlock(&metrics_mutex);
			free(latency_summary);
		}
		if (lockResult == 0) {
			pthread_mutex_unlock(&req_mutex);
		}
    	usleep(10000);
    }
}

void event_upload_thread() 
{
	pthread_mutex_lock(&metrics_mutex);
	metrics_array = malloc(metrics_capacity * sizeof(char*));
	pthread_mutex_unlock(&metrics_mutex);
	
	struct cnfPut cnfput;
    if (cnfPut_init(&cnfput, PIPE_PATH)){
		fprintf(stderr, "CnfPut init fail: %s\n", PIPE_PATH);
	}

    while (1) {
		int i = 0;
		time_t startTime = time(NULL);
		char *latency_summaries = malloc(JSON_BUFFER_SIZE);
	    memset(latency_summaries, 0x0, JSON_BUFFER_SIZE);
		pthread_mutex_lock(&metrics_mutex);
		printf("Count: %d\n", metrics_num);
		if (metrics_num > 0) {
			for (i = 0; i < metrics_num; i++) {
				//printf("combine: %s\n", metrics_array[i]);
				if (strlen(latency_summaries) + strlen(metrics_array[i]) >= JSON_BUFFER_SIZE){
					if (cnfPut_puts(&cnfput, latency_summaries)){
						fprintf(stderr, "CnfPut put fail: %s\n", PIPE_PATH);
						cnfPut_destroy(&cnfput);
					}
					printf("latency_summaries: %s\n", latency_summaries);
					memset(latency_summaries, 0x0, JSON_BUFFER_SIZE);
				}
				sprintf(latency_summaries + strlen(latency_summaries), "%s", metrics_array[i]);
				if (i < metrics_num - 1 && (strlen(latency_summaries) + strlen(metrics_array[i+1]) < JSON_BUFFER_SIZE)) {
					sprintf(latency_summaries + strlen(latency_summaries), "%s", "\n");
				}
        	}

			if (cnfPut_puts(&cnfput, latency_summaries)){
				fprintf(stderr, "CnfPut put fail: %s\n", PIPE_PATH);
				cnfPut_destroy(&cnfput);
			}

			for (i = 0; i < metrics_num; i++) {
        		free(metrics_array[i]);
    		}
    		free(metrics_array);
			metrics_num = 0;
			metrics_capacity = 10;
			metrics_array = malloc(metrics_capacity * sizeof(char*));
		}
		pthread_mutex_unlock(&metrics_mutex);
		free(latency_summaries);

        time_t endTime = time(NULL);
        time_t sleepTime = 6 - (endTime - startTime);
        if (sleepTime > 0) {
            sleep(sleepTime);
        }
    }	
}

static void iosdiag_upload_result(void *ctx, int cpu, void *data, __u32 size)
{
	struct iosdiag_req *iop = (struct iosdiag_req *)data;

	if (over_threshold(iop)) {
		// set_check_time_date();
		// char *latency_summary = malloc(JSON_BUFFER_SIZE);
		// memset(latency_summary, 0x0, JSON_BUFFER_SIZE);
		pthread_mutex_lock(&req_mutex);

		if (req_array_length >= req_capacity) {
        	req_capacity *= 2;
			req_array = (struct iosdiag_req*)realloc(req_array, req_capacity * sizeof(struct iosdiag_req));
    	}

		req_array[req_array_length] = *iop;
		req_array_length++;

		pthread_mutex_unlock(&req_mutex);
	}
}

static void iosdiag_collect(void)
{
	struct perf_buffer_opts pb_opts = {};
	struct perf_buffer *pb;

	pb_opts.sample_cb = iosdiag_store_result;
	pb = perf_buffer__new(iosdiag_maps_notify, 1, &pb_opts);

	printf("running...");
	fflush(stdout);
	g_json_buf = malloc(JSON_BUFFER_SIZE);
	memset(g_json_buf, 0x0, JSON_BUFFER_SIZE);
	while (!g_stop)
		perf_buffer__poll(pb, 100);
	perf_buffer__free(pb);
	free(g_json_buf);
	printf("done\n");
}

static void iosdiag_collect_normalization(void)
{
	struct perf_buffer_opts pb_opts = {};
	struct perf_buffer *pb;

	pb_opts.sample_cb = iosdiag_upload_result;

	pb = perf_buffer__new(iosdiag_maps_notify, 1, &pb_opts);
	printf("running...\n");
	fflush(stdout);
	g_json_buf = malloc(JSON_BUFFER_SIZE);
	memset(g_json_buf, 0x0, JSON_BUFFER_SIZE);

	while (!g_stop)
		perf_buffer__poll(pb, 100);

	perf_buffer__free(pb);
	free(g_json_buf);
	printf("done\n");
}

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
	if (enable_debug_log())
		return vfprintf(stderr, format, args);
	return 0;
}

static void iosdiag_stop(int signo)
{
    //printf("iosdiag stop!\n");
    g_stop = 1;
}

typedef struct {
    char bpf_name[20];
	int bpf_load_map;
    int bpf_period;
} LoadIosdiagArgs;

#define LOAD_IOSDIAG_BPF(name, load_map, period)							\
({												\
	__label__ out;										\
	int __ret = 0;										\
	printf("start %s load bpf\n", #name);							\
	name = name##_bpf__open();								\
	if (!name) {										\
		printf("load bpf error\n");							\
		printf("load %s bpf fail\n", #name);						\
		__ret = -1;									\
		goto out;									\
	}											\
	if (name##_bpf__load(name)) {								\
		printf("load bpf prog error\n");						\
		printf("load %s bpf fail\n", #name);						\
		name##_bpf__destroy(name);							\
		__ret = -1;									\
		goto out;									\
	}											\
	if (name##_bpf__attach(name)) {								\
		printf("attach bpf prog error\n");						\
		printf("load %s bpf fail\n", #name);						\
		name##_bpf__destroy(name);							\
		__ret = -1;									\
		goto out;									\
	}											\
	if (load_map) {										\
		iosdiag_map = bpf_map__fd(name->maps.iosdiag_maps);				\
		iosdiag_maps_notify = bpf_map__fd(name->maps.iosdiag_maps_notify);	\
		iosdiag_maps_targetdevt = bpf_map__fd(name->maps.iosdiag_maps_targetdevt);	\
	}											\
	if (!__ret)										\
		printf("load %s bpf success\n", #name);						\
		name##_bpf_load = 1;									\
		if (period > 0) {		\
			pthread_t attach_thread;			\
			LoadIosdiagArgs *args = malloc(sizeof(LoadIosdiagArgs)); \
			strcpy(args->bpf_name, #name);	\
			args->bpf_load_map = load_map;		\
			args->bpf_period = period;		\
    		pthread_create(&attach_thread, NULL, attach_periodically, args); 	 \
		}		\
out:												\
	__ret;											\
})

void* attach_periodically(void* args) 
{
    LoadIosdiagArgs* attach_args = (LoadIosdiagArgs*)args;
	char* name = attach_args->bpf_name;
	int load_map = attach_args->bpf_load_map;
	int period = attach_args->bpf_period;
	// printf("arg: name = %s, load_map = %d, period = %d\n", name, load_map, period);										

    while (1) {
		pthread_mutex_lock(&mutex);
        if (exit_flag) {
            pthread_mutex_unlock(&mutex);
            break;
        }
        pthread_mutex_unlock(&mutex);

		sleep(period);
		if (iosdiag_virtblk_bpf_load && !strcmp(name, "iosdiag_virtblk")){
			iosdiag_virtblk_bpf__detach(iosdiag_virtblk);
			printf("dettach %s\n", "iosdiag_virtblk");
		}else if (iosdiag_nvme_bpf_load && !strcmp(name, "iosdiag_nvme")){
			iosdiag_nvme_bpf__detach(iosdiag_nvme);
			printf("dettach %s\n", "iosdiag_nvme");
		}else if (iosdiag_scsi_bpf_load && !strcmp(name, "iosdiag_scsi")){
			iosdiag_scsi_bpf__detach(iosdiag_scsi);
			printf("dettach %s\n", "iosdiag_scsi");
		}else if (iosdiag_scsi_mq_bpf_load && !strcmp(name, "iosdiag_scsi_mq")){
			iosdiag_scsi_mq_bpf__detach(iosdiag_scsi_mq);
			printf("dettach %s\n", "iosdiag_scsi_mq");
		}

		sleep(period);
		if (!strcmp(name, "iosdiag_virtblk")) {
			printf("restart %s load bpf\n", "iosdiag_virtblk");						
			if (iosdiag_virtblk_bpf__attach(iosdiag_virtblk)) {								
				printf("load %s bpf prog fail\n", "iosdiag_virtblk");						
				iosdiag_virtblk_bpf__destroy(iosdiag_virtblk);	
				return NULL;						
			}
			if (load_map) {										
				iosdiag_map = bpf_map__fd(iosdiag_virtblk->maps.iosdiag_maps);				
				iosdiag_maps_notify = bpf_map__fd(iosdiag_virtblk->maps.iosdiag_maps_notify);	
				iosdiag_maps_targetdevt = bpf_map__fd(iosdiag_virtblk->maps.iosdiag_maps_targetdevt);	
			}		
		}else if (!strcmp(name, "iosdiag_nvme")){
			printf("restart %s load bpf\n", "iosdiag_nvme");																	
			if (iosdiag_nvme_bpf__attach(iosdiag_nvme)) {								
				printf("attach bpf prog error\n");						
				printf("load %s bpf fail\n", "iosdiag_nvme");						
				iosdiag_nvme_bpf__destroy(iosdiag_nvme);
				return NULL;							
			}
			if (load_map) {										
				iosdiag_map = bpf_map__fd(iosdiag_nvme->maps.iosdiag_maps);				
				iosdiag_maps_notify = bpf_map__fd(iosdiag_nvme->maps.iosdiag_maps_notify);	
				iosdiag_maps_targetdevt = bpf_map__fd(iosdiag_nvme->maps.iosdiag_maps_targetdevt);	
			}		
		}else if (!strcmp(name, "iosdiag_scsi")){
			printf("restart %s load bpf\n", "iosdiag_scsi");																	
			if (iosdiag_scsi_bpf__attach(iosdiag_scsi)) {								
				printf("attach bpf prog error\n");						
				printf("load %s bpf fail\n", "iosdiag_scsi");						
				iosdiag_scsi_bpf__destroy(iosdiag_scsi);							
				return NULL;									
			}
			if (load_map) {										
				iosdiag_map = bpf_map__fd(iosdiag_scsi->maps.iosdiag_maps);				
				iosdiag_maps_notify = bpf_map__fd(iosdiag_scsi->maps.iosdiag_maps_notify);	
				iosdiag_maps_targetdevt = bpf_map__fd(iosdiag_scsi->maps.iosdiag_maps_targetdevt);
			}
		}else if (!strcmp(name, "iosdiag_scsi_mq")){
			printf("restart %s load bpf\n", "iosdiag_scsi_mq");																
			if (iosdiag_scsi_mq_bpf__attach(iosdiag_scsi_mq)) {								
				printf("attach bpf prog error\n");						
				printf("load %s bpf fail\n", "iosdiag_scsi_mq");						
				iosdiag_scsi_mq_bpf__destroy(iosdiag_scsi_mq);							
				return NULL;					
			}
			if (load_map) {										
				iosdiag_map = bpf_map__fd(iosdiag_scsi_mq->maps.iosdiag_maps);				
				iosdiag_maps_notify = bpf_map__fd(iosdiag_scsi_mq->maps.iosdiag_maps_notify);	
				iosdiag_maps_targetdevt = bpf_map__fd(iosdiag_scsi_mq->maps.iosdiag_maps_targetdevt);	
			}		
		}
	}
	pthread_exit(NULL);
}

static unsigned int get_devt_by_devname(char *devname)
{
	char sys_file[64];
	char cmd[128];
	char dev[16];
	FILE *fp;
	int major = 0, minor = 0;

	sprintf(sys_file, "/sys/block/%s/dev", devname);
	if (access(sys_file, F_OK))
		sprintf(sys_file, "/sys/block/*/%s/../dev", devname);

	sprintf(cmd, "cat %s 2>/dev/null", sys_file);
	if ((fp = popen(cmd, "r")) == NULL) {
		fprintf(stderr, "exec \'%s\' fail\n", cmd);
		return 0;
	}

	while (fgets(dev, sizeof(dev) - 1, fp)) {
		if (sscanf(dev, "%d:%d", &major, &minor) != 2) {
			pclose(fp);
			return 0;
		}
	}
	pclose(fp);
	return ((major << 20) | minor);
}

static char *get_module_name_by_devname(char *devname)
{
	char sys_file[64] = {0};
	char file_path[PATH_MAX] = {0};
	int ret;

	sprintf(sys_file, "/sys/class/block/%s", devname);
	ret = readlink(sys_file, file_path, PATH_MAX);
	if (ret < 0 || ret >= PATH_MAX)
		return "none";
	if (strstr(file_path, "virtio"))
		return "virtblk";
	else if (strstr(file_path, "nvme"))
		return "nvme";
	else if (strstr(file_path, "target"))
		return "scsi";
	return "none";
}

int iosdiag_init(char *devname, unsigned int attach_interval)
{
	struct rlimit r = {RLIM_INFINITY, RLIM_INFINITY};
	int key = 0;
	unsigned int target_devt = get_devt_by_devname(devname);
	char *module_name = get_module_name_by_devname(devname);
	if (attach_interval > 0) {
		signal(SIGINT, signal_handler);				
	}
	setrlimit(RLIMIT_MEMLOCK, &r);

	libbpf_set_print(libbpf_print_fn);
	if (!strcmp(module_name, "virtblk")) {
		if (LOAD_IOSDIAG_BPF(iosdiag_virtblk, 1, attach_interval))
				return -1;
	} else if (!strcmp(module_name, "nvme")) {
		if (LOAD_IOSDIAG_BPF(iosdiag_nvme, 1, attach_interval))
			return -1;
	} else if (!strcmp(module_name, "scsi")) {
		if (LOAD_IOSDIAG_BPF(iosdiag_scsi, 1, attach_interval)) {
			if (LOAD_IOSDIAG_BPF(iosdiag_scsi_mq, 1, attach_interval))
				return -1;
		}
	} else {
		if (LOAD_IOSDIAG_BPF(iosdiag_virtblk, 1, attach_interval)) {
			if (LOAD_IOSDIAG_BPF(iosdiag_nvme, 1, attach_interval)) {
				if (LOAD_IOSDIAG_BPF(iosdiag_scsi, 1, attach_interval)) {
					if (LOAD_IOSDIAG_BPF(iosdiag_scsi_mq, 1, attach_interval))
						return -1;
				}
			} else {
				if (LOAD_IOSDIAG_BPF(iosdiag_scsi, 0, attach_interval)) {
					LOAD_IOSDIAG_BPF(iosdiag_scsi_mq, 0, attach_interval);
				}
			}
		} else {
			LOAD_IOSDIAG_BPF(iosdiag_nvme, 0, attach_interval);
			if (LOAD_IOSDIAG_BPF(iosdiag_scsi, 0, attach_interval)) {
				LOAD_IOSDIAG_BPF(iosdiag_scsi_mq, 0, attach_interval);
			}
		}
	}
	if (iosdiag_virtblk_bpf_load)
		iosdiag_virtblk_map =
			bpf_map__fd(iosdiag_virtblk->maps.iosdiag_virtblk_maps);
	if (target_devt)
		bpf_map_update_elem(iosdiag_maps_targetdevt, &key, &target_devt, BPF_ANY);
     
    return 0;
}

int iosdiag_run(int timeout, int mode, char *output_file)
{
	signal(SIGINT, iosdiag_stop);
	signal(SIGALRM, iosdiag_stop);
	signal(SIGALRM, signal_handler);
	alarm(timeout);

	pthread_t aggregator_thread;
	pthread_create(&aggregator_thread, NULL, event_aggregator, NULL);

	if (mode == DIAGNOSTIC_MODE) {
		char filepath[256];
		char cmd[272];

		if (strlen(output_file) > (sizeof(filepath) - 1)) {
			printf("error: output file name(%s) too large(max %lu bytes)\n",
				output_file, sizeof(filepath));
			return -1;
		}
		strcpy(filepath, output_file);
		sprintf(cmd, "mkdir %s -p", dirname(filepath));
		exec_shell_cmd(cmd);
		g_log_fd = open(output_file, O_RDWR | O_CREAT, 0755);
		if (g_log_fd < 0) {
			printf("error: create output file \"%s\" fail\n", output_file);
			return -1;
		}
		iosdiag_collect();
	}else{
		pthread_t export_metrics_thread;
		pthread_create(&export_metrics_thread, NULL, event_upload_thread, NULL);
		iosdiag_collect_normalization();
		pthread_join(export_metrics_thread, NULL);
	}

	close(g_log_fd);
	return 0;
}

void iosdiag_exit(char *module_name)
{
	if (iosdiag_virtblk_bpf_load)
		iosdiag_virtblk_bpf__destroy(iosdiag_virtblk);
	if (iosdiag_nvme_bpf_load)
		iosdiag_nvme_bpf__destroy(iosdiag_nvme);
	if (iosdiag_scsi_bpf_load)
		iosdiag_scsi_bpf__destroy(iosdiag_scsi);
	if (iosdiag_scsi_mq_bpf_load)
		iosdiag_scsi_mq_bpf__destroy(iosdiag_scsi_mq);
}

