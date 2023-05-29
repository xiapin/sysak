#include "meminfo.h"
#include "kill.h"
#include "metric.h"
#include <math.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#define BUFSIZE 256

 long get_watermark_scale_factor(void)
{
    int fd; 
    char path[] = "/proc/sys/vm/watermark_scale_factor";
    char buffer[32];
    long watermark_scale_factor = 0;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        return 0;
    }   

    if (read(fd, buffer, sizeof(buffer)) < 0) {
        close(fd);
        return 0;
    }

    close(fd);

    watermark_scale_factor = atol(buffer);

    printf("watermark_scale_factor: %ld ", watermark_scale_factor);

    return watermark_scale_factor;
}

 long get_min_free(void)
{
    int fd;
    char path[] = "/proc/sys/vm/min_free_kbytes";
    char buffer[32];
    long min_free_kbytes;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("Failed to open file");
        return -1;
    }

    if (read(fd, buffer, sizeof(buffer)) < 0) {
        printf("Failed to read file");
        return -1;
    }

    close(fd);

    min_free_kbytes = atol(buffer);

    printf("min_free_kbytes: %ld ", min_free_kbytes);

    return min_free_kbytes;
}

int get_watermark(poll_loop_args_t *poll, meminfo_t *m)
{
    long min_free_kbyte;
    long watermark_scale_factor;
    long tmp = 0;

    min_free_kbyte = get_min_free();
    poll->min = min_free_kbyte;

    watermark_scale_factor = get_watermark_scale_factor();
    if (watermark_scale_factor) {
        tmp = m->MemTotalKiB*watermark_scale_factor/10000;
    }

    if ((min_free_kbyte>>2) > tmp)
        tmp = min_free_kbyte>>2;

    poll->low = tmp + min_free_kbyte;
    poll->high = tmp*2 + min_free_kbyte;

    printf("min:%ld low:%ld high:%ld\n", poll->min, poll->low, poll->high);
    return 0;
}


float factor_x(float interval,float avg)
{
    return 1.0/expf(interval/avg);
}

//load1 = load0 * e + active * (1 - e)
float avg_x(float curr, float prev, float factor)
{
    return prev*factor + curr*(1-factor);
}

static void read_cpu_stat(struct cpu_stat *cs) {
    FILE *fp = fopen("/proc/stat", "r");
    if (fp == NULL) {
        printf("open /proc/stat error\n");
        exit(1); 
    }
    fscanf(fp, "cpu %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld",
           &cs->user, &cs->nice, &cs->system, &cs->idle, &cs->iowait,
           &cs->irq, &cs->softirq, &cs->steal, &cs->guest, &cs->guest_nice);
    fclose(fp);
}


static void diff_cpu_stat(struct cpu_stat *prev, struct cpu_stat *curr, struct cpu_stat *diff) {
    diff->user = curr->user - prev->user;
    diff->nice = curr->nice - prev->nice;
    diff->system = curr->system - prev->system;
    diff->idle = curr->idle - prev->idle;
    diff->iowait = curr->iowait - prev->iowait;
    diff->irq = curr->irq - prev->irq;
    diff->softirq = curr->softirq - prev->softirq;
    diff->steal = curr->steal - prev->steal;
    diff->guest = curr->guest - prev->guest;
    diff->guest_nice = curr->guest_nice - prev->guest_nice;
}

static int calc_percent(struct cpu_stat *diff, poll_loop_args_t *poll) {
    
    int interval = poll->report_interval_ms/1000;
    float curr = 0.0;
    float prev_avg = 0.0;

    long total = diff->user + diff->nice + diff->system + diff->idle + diff->iowait +
                 diff->irq + diff->softirq + diff->steal + diff->guest +
                 diff->guest_nice;
    if (total == 0) {
        memset(&poll->cstat_util, 0, sizeof(poll->cstat_util));
    }
    else {
        poll->cstat_util.iowait = (float)diff->iowait / total * 100.0;
        poll->cstat_util.user = (float)diff->user / total * 100.0;
        poll->cstat_util.system = (float)diff->system / total * 100.0;
        poll->cstat_util.idle = (float)diff->idle / total * 100.0;
        /* for iowait avg */ 
        prev_avg = poll->cstat_util.iowait_avg10;
        curr = poll->cstat_util.iowait;
        poll->cstat_util.iowait_avg10 = avg_x(curr, prev_avg, factor_x(interval,10));

        prev_avg = poll->cstat_util.iowait_avg30;
        poll->cstat_util.iowait_avg30 = avg_x(curr, prev_avg, factor_x(interval,30));
        
        prev_avg = poll->cstat_util.iowait_avg60;
        poll->cstat_util.iowait_avg60 = avg_x(curr, prev_avg, factor_x(interval,60));
	
	/* for system avg */ 
        prev_avg = poll->cstat_util.system_avg10;
        curr = poll->cstat_util.system;
        poll->cstat_util.system_avg10 = avg_x(curr, prev_avg, factor_x(interval,10));

        prev_avg = poll->cstat_util.system_avg30;
        poll->cstat_util.system_avg30 = avg_x(curr, prev_avg, factor_x(interval,30));
        
        prev_avg = poll->cstat_util.system_avg60;
        poll->cstat_util.system_avg60 = avg_x(curr, prev_avg, factor_x(interval,60));

    }
    return 0;
}

int get_cpu_stat(poll_loop_args_t *poll)
{
    struct cpu_stat curr, diff;

    read_cpu_stat(&curr);
    diff_cpu_stat(&poll->cstat_prev, &curr, &diff);
    calc_percent(&diff, poll);
    poll->cstat_prev = curr;
    //printf("user: %.2f%% iowait: %.2f%%\n", poll->cstat_util.user,poll->cstat_util.iowait);
    //printf("avg10: %.2f%% avg30: %.2f%% avg60:%.2f%% \n", poll->cstat_util.iowait_avg10,poll->cstat_util.iowait_avg30, poll->cstat_util.iowait_avg60);
    return 0;
}

int event_init(poll_loop_args_t *poll)
{
    char buf[BUFSIZE];
    int ret = 0; 
    poll->poll_fd = eventfd(0, 0);
    
    snprintf(buf, BUFSIZE, "%s", "/sys/fs/cgroup/memory/cgroup.event_control");    
    poll->eventc_fd = open(buf, O_WRONLY);

    snprintf(buf, BUFSIZE, "%s","/sys/fs/cgroup/memory/memory.pressure_level");
    poll->pressure_fd = open(buf, O_RDONLY);

    snprintf(buf, BUFSIZE, "%d %d low", poll->poll_fd, poll->pressure_fd);
    write(poll->eventc_fd, buf, strlen(buf));
    
    poll->pfd.fd = poll->poll_fd;
    poll->pfd.events = POLLIN; 
}

int event_poll(poll_loop_args_t *polls, int timeout)
{
    unsigned long u;
    int ret = 0;

    ret = poll(&polls->pfd, 1, timeout);
    if (ret > 0) {
        read(polls->poll_fd, &u, sizeof(u));
    }
    return ret;
}

int event_uninit(poll_loop_args_t *poll)
{
    close(poll->poll_fd);
    close(poll->eventc_fd);
    close(poll->pressure_fd);
    return 0;
}
int metric_init(poll_loop_args_t *poll)
{
    memset(&poll->cstat_util, 0, sizeof(poll->cstat_util));
    memset(&poll->cstat_prev, 0, sizeof(poll->cstat_prev));
    meminfo_t m = parse_meminfo();
    get_watermark(poll, &m);
    event_init(poll);
    
    return 0;
}

int metric_exit(poll_loop_args_t *poll)
{
    event_uninit(poll);
    return 0;
}
