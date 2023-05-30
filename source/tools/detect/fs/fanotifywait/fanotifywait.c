#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/fanotify.h>

#define PATH_LEN 32
#define KVERSION 64
#define COMM_LEN 64
#define BUF_SIZE 512
#define METADATA 4096

char *log_dir = "/var/log/sysak/fanotifywait";
char *log_file = "/var/log/sysak/fanotifywait/fanotifywait.log";
unsigned long runtime = 60;
char filename[BUF_SIZE] = {0};
char kern_version[KVERSION] = {0};
char machine[KVERSION] = {0};
static volatile sig_atomic_t exiting;

struct option longopts[] = {
    { "filename", no_argument, NULL, 'f' },
	{ "runtime", no_argument, NULL, 's' },
    //{ "event", no_argument, NULL, 'e' },
    { "help", no_argument, NULL, 'h' },
    { 0, 0, 0, 0},
};

static void usage(void)
{
	fprintf(stdout,
	        "Usage: sysak fanotifywait [options] [args]\n"
            "Options:\n"
			"    -s		specify how long to run, default= 60s\n"
            //"    -e		specify fanotify monitor even, default=open\n"
            "    -f		specify monitor file or dir\n"
            "    -h		help info\n"
            "example:\n" 
            "sysak fanotifywait -f ./test.txt -s 10  #monitor file\n"
            "sysak fanotifywait -f ./test -s 10  #monitor file\n");
	exit(EXIT_FAILURE);
}

static void sig_int(int signo)
{
	exiting = 1;
}

static void sig_alarm(int signo)
{
	exiting = 1;
}

static int prepare_dictory(char *path)
{
	int ret = 0;

	if (access(path,0) != 0)
        ret = mkdir(path, 0777);

	return ret;
}

static void kern_release(void)
{
    struct utsname name;

    if (uname(&name) == -1) {
        printf("cannot get system version\n");
        return;
    }
    strncpy(kern_version, name.release, sizeof(name.release));
}

void task_name(pid_t pid, char *name)
{
    char proc_pid_path[BUF_SIZE];
    char buf[BUF_SIZE];

    sprintf(proc_pid_path, "/proc/%d/status", pid);
    FILE *status_fp = fopen(proc_pid_path, "r");

    if(status_fp != NULL) {
        fgets(buf, BUF_SIZE-1, status_fp) == NULL;
        fclose(status_fp);
        sscanf(buf,"%*s %s", name);
    }
}

int main(int argc, char** argv)
{
    int fan, opt, g_runtime;
    int mount_fd, event_fd;
    char buf[METADATA];
    char fdpath[PATH_LEN];
    char path[PATH_MAX + 1];
    char log_line[BUF_SIZE];
    char name[COMM_LEN];
    time_t t;
    struct tm *lt;
    struct stat s_buf;
    struct file_handle *file_handle;
    struct fanotify_response response;
    ssize_t buflen, linklen;
    struct fanotify_event_metadata *metadata;
    struct fanotify_event_info_fid *fid;
    FILE *log_fd = NULL;

    prepare_dictory(log_dir);
    kern_release();
    time(&t);

    while ((opt = getopt_long(argc, argv, "s:f:h", longopts, NULL)) != -1) {
		switch (opt) {
			case 's':
                if (optarg)
                    runtime = (int)strtoul(optarg, NULL, 10);
				break;
            case 'f':
				strncpy(filename, optarg, sizeof(filename));
				break;
            /*case 'e':
                if (optarg)
                    g_thresh = (int)strtoul(optarg, NULL, 10)*1000*1000;
				break;
                */
			case 'h':
                usage();
                break;
			default:
                printf("must have parameter\n");
                usage();
                break;
        }
	}

    if (signal(SIGINT, sig_int) == SIG_ERR ||
		signal(SIGALRM, sig_alarm) == SIG_ERR) {
		fprintf(stderr, "can't set signal handler: %s\n", strerror(errno));
		return 0;
	}

    stat(filename,&s_buf);
    if (S_ISDIR(s_buf.st_mode))
        mount_fd = open(filename, O_DIRECTORY | O_RDONLY);
    else
        mount_fd = open(filename, O_RDONLY);

    if (mount_fd == -1) {
        perror(filename);
        exit(EXIT_FAILURE);
    }

    fan = fanotify_init(FAN_CLASS_NOTIF | FAN_CLASS_CONTENT | FAN_NONBLOCK, O_RDWR);
    if(fan == -1) {
        perror("fanotify_init");
        exit(EXIT_FAILURE);
    }
    
    int ret = fanotify_mark(fan,
                            FAN_MARK_ADD,
                            FAN_CLOSE | FAN_ACCESS_PERM | FAN_OPEN_PERM | FAN_MODIFY | FAN_EVENT_ON_CHILD,
                            AT_FDCWD,
                            filename
                            );
    if(ret == -1) {
        perror("fanotify_mark");
        exit(EXIT_FAILURE);
    }

    if (runtime)
		alarm(runtime); 

    while(!exiting) {
        buflen = read(fan, buf, sizeof(buf));
        metadata = (struct fanotify_event_metadata*)&buf;

        for (; FAN_EVENT_OK(metadata, buflen); metadata = FAN_EVENT_NEXT(metadata, buflen)) {
            lt = localtime(&t);
            memset(log_line, 0, sizeof(log_line));
            sprintf(log_line, "%d/%d/%d %d:%d:%d " ,lt->tm_year+1900, lt->tm_mon+1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec);
            task_name(metadata->pid, name);
            sprintf(log_line, "%s %s:%d ",log_line, name, metadata->pid);

            switch(metadata->mask){
                case FAN_ACCESS_PERM:
                    sprintf(log_line, "%s %s", log_line, "ACCESS ");
                    break;
                case FAN_OPEN_PERM:
                    sprintf(log_line, "%s %s", log_line, "OPEN ");
                    break;
                case FAN_CLOSE_WRITE:
                    sprintf(log_line, "%s %s", log_line, "CLOSE_WRITE ");
                    break;;
                case FAN_CLOSE_NOWRITE:
                    sprintf(log_line, "%s %s", log_line, "CLOSE_NOWRITE ");
                    break;
                case FAN_MODIFY:
                    sprintf(log_line, "%s %s", log_line, "MODIFY ");
                    break;
                case FAN_CLOSE:
                    sprintf(log_line, "%s %s", log_line, "CLOSE: ");
                    break;
            }

	        if (metadata->mask & (FAN_OPEN_PERM | FAN_ACCESS_PERM)) {
                /* Allow file to be opened and access */

                response.fd = metadata->fd;
                response.response = FAN_ALLOW;
                write(fan, &response,
                     sizeof(struct fanotify_response));
               }

            sprintf(fdpath, "/proc/self/fd/%d", metadata->fd);
            linklen = readlink(fdpath, path, sizeof(path) - 1);
            if (linklen == -1) {
                perror("readlink");
            }
            path[linklen] = '\0';
            sprintf(log_line, "%s %s", log_line, path);

            log_fd = fopen(log_file, "a+");
            fprintf(log_fd, "%s\n", log_line);
            fclose(log_fd);

            close(metadata->fd);
            metadata = FAN_EVENT_NEXT(metadata, buflen);
        }
    }
}
