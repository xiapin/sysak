#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <ctype.h>
#include <sys/resource.h>

#define MAX_COMM 16 // 最大路径长度
#define MAX_PATH 256 // 最大路径长度
#define MAX_LINE 128 // 最大行长度
#define MAX_PROC 10 // 最大进程数
#define FD_THRESHOLD 0.6 // fd利用率阈值
#define PID_THRESHOLD 0.6 // pid利用率阈值
#define ROOT_THRESHOLD 0.6 // 根分区利用率阈值
#define INODE_THRESHOLD 0.6 // 根分区inode利用率阈值

struct process {
    int pid;
    int fd;
    char name[MAX_COMM];
};

int get_process_file_limit(void)
{
    struct rlimit rlim;
    int file_max = 455350;

    if (getrlimit(RLIMIT_NOFILE, &rlim) == 0) {
        file_max = rlim.rlim_cur; 
    }   
    return file_max;
}

int check_fd_usage(double threshold) {
    FILE *fp;
    char line[MAX_LINE];
    int total, used, free;
    double ratio;
    int ret = 0; 
    fp = fopen("/proc/sys/fs/file-nr", "r");
    if (fp == NULL) {
        perror("fopen");
        exit(1);
    }
    fgets(line, MAX_LINE, fp);
    fclose(fp);
    sscanf(line, "%d %*d %d", &used,&total);
    ratio = (double)used / total;
    ret = !!(ratio > threshold);
    if (ret)
        printf("file-max:%d file-used:%d pid-max:%d\n", total, used, get_process_file_limit());
    return ret;
}

int check_pid_usage(double threshold) {
    FILE *fp;
    char line[MAX_LINE];
    int total, used, free;
    double ratio;

    fp = fopen("/proc/sys/kernel/pid_max", "r");
    if (fp == NULL) {
        perror("fopen");
        exit(1);
    }
    fgets(line, MAX_LINE, fp);
    fclose(fp);
    sscanf(line, "%d", &total);
    fp = fopen("/proc/sys/kernel/pid_max", "r");
    if (fp == NULL) {
        perror("fopen");
        exit(1);
    }
    fgets(line, MAX_LINE, fp);
    fclose(fp);
    sscanf(line, "%d", &used);
    free = total - used;
    ratio = (double)used / total;
    if (ratio > threshold) {
        return 1;
    } else {
        return 0;
    }
}

int check_root_usage(double threshold) {
    struct statvfs buf;
    unsigned long total, used, free;
    double ratio;

    if (statvfs("/", &buf) != 0) {
        perror("statvfs");
        exit(1);
    }
    total = buf.f_blocks * buf.f_frsize;
    free = buf.f_bfree * buf.f_frsize;
    used = total - free;
    ratio = (double)used / total;
    if (ratio > threshold) {
        return 1;
    } else {
        return 0;
    }
}

int check_inode_usage(double threshold) {
    struct statvfs buf;
    unsigned long total, used, free;
    double ratio;

    if (statvfs("/", &buf) != 0) {
        perror("statvfs");
        exit(1);
    }
    total = buf.f_files;
    free = buf.f_ffree;
    used = total - free;
    ratio = (double)used / total;
    if (ratio > threshold) {
        return 1;
    } else {
        return 0;
    }
}

int pid_comm(char *name, int pid)
{
    int len = 0;
    FILE *fp;
    char path[MAX_PATH];
    sprintf(path, "/proc/%d/comm", pid);
    fp = fopen(path, "r");
    if (fp == NULL)
        return -1;
    fgets(name, MAX_COMM, fp);
    len = strlen(name);
    if (name[len -1] == '\n') {
        name[len -1] = '\0';
    }
    fclose(fp);

}
void check_top_fd_processes(struct process *top) {
    DIR *dpp;
    DIR *dp;
    struct dirent *entry;
    char path[MAX_PATH];
    int pid, fd;
    char name[MAX_COMM];
    FILE *fp;
    int i, j;
    int count;
    struct process temp;
    int len = 0;
    dpp = opendir("/proc");
    if (dpp == NULL) {
        perror("opendir");
        exit(1);
    }
    count = 0;
    while ((entry = readdir(dpp)) != NULL) {
        if (isdigit(entry->d_name[0])) {
            pid = atoi(entry->d_name);
            sprintf(path, "/proc/%d/fd", pid);
            fd = 0;
            dp = opendir(path);
            if (dp == NULL) {
                continue;
            }
            while ((entry = readdir(dp)) != NULL) {
                if (isdigit(entry->d_name[0])) {
                    fd++;
                }
            }
            closedir(dp);
            if (count < MAX_PROC) {
                pid_comm(name, pid);
                top[count].pid = pid;
                top[count].fd = fd;
                strcpy(top[count].name, name);
                count++;
            } else {
                if (fd > top[MAX_PROC - 1].fd) {
                    pid_comm(name, pid);
                    top[MAX_PROC - 1].pid = pid;
                    top[MAX_PROC - 1].fd = fd;
                    strcpy(top[MAX_PROC - 1].name, name);
                    for (i = MAX_PROC - 2; i >= 0; i--) {
                        // 如果当前进程的fd数大于前一个进程的fd数，交换位置
                        if (top[i + 1].fd > top[i].fd) {
                            temp = top[i + 1];
                            top[i + 1] = top[i];
                            top[i] = temp;
                        } else {
                            break;
                        }
                    }
                }
            }
        }
    }
    closedir(dpp);
}

int main(int argc, char **argv)
{
    int i = 0;
    double threshold = 0.5;
    int result = 0;
    struct process *top = NULL;

    if (argc == 1) {
        printf("Usage: %s [fd|pid|root|inode] [threshold]\n", argv[0]);
        exit(0);
    }
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "fd") == 0) {
            if (i + 1 < argc) {
                threshold = atof(argv[i + 1]);
                if (threshold > 0 && threshold < 1) {
                i++;
            } else {
                threshold = FD_THRESHOLD;
            }
        } else {
            threshold = FD_THRESHOLD;
        }
        result = check_fd_usage(threshold);
        if (result == 1) {
            top = (struct process *)malloc(MAX_PROC * sizeof(struct process));
            if (!top) {
                printf("alloc proccess top failed \n");
                continue;
            }
            check_top_fd_processes(top);
            for (i = 0; i < MAX_PROC; i++) {
                printf("pid: %d, comm: %s, fd: %d\n", top[i].pid, top[i].name, top[i].fd);
            }
            if(top) {
                free(top);
                top = NULL;
            }
        } else 
            printf("fd check ok\n");
    }
    else if (strcmp(argv[i], "pid") == 0) {
        if (i + 1 < argc) {
            threshold = atof(argv[i + 1]);
            if (threshold > 0 && threshold < 1) {
                i++;
            } else {
                threshold = PID_THRESHOLD;
            }
        } else {
            threshold = PID_THRESHOLD;
        }
        //result = check_pid_usage(threshold);
        printf("pid usage: %d\n", result);
    }
    else if (strcmp(argv[i], "root") == 0) {
        if (i + 1 < argc) {
            threshold = atof(argv[i + 1]);
            if (threshold > 0 && threshold < 1) {
                i++;
            } else {
                threshold = ROOT_THRESHOLD;
            }
        } else {
            threshold = ROOT_THRESHOLD;
        }
        //result = check_root_usage(threshold);
        printf("root usage: %d\n", result);
    }
    else if (strcmp(argv[i], "inode") == 0) {
        if (i + 1 < argc) {
            threshold = atof(argv[i + 1]);
            if (threshold > 0 && threshold < 1) {
                i++;
            } else {
                threshold = INODE_THRESHOLD;
            }
        } else {
            threshold = INODE_THRESHOLD;
        }
        //result = check_inode_usage(threshold);
        printf("inode usage: %d\n", result);
    }
    else {
        printf("Usage: [fd|pid|root|inode] [threshold]\n"
           "fd: fd usage check \n"
           "pid: pid usage check\n"
           "inode: inode usage check\n"
           "threshold: percent of max \n"
           "example: sysak fd 0.5 \n");
        exit(1);
    }
   }
   return 0;
}

