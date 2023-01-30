#include <stdio.h>
#include <argp.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include "syscall_helpers.h"
#include "bpf/appnoise.skel.h"
#include "appnoise.h"

static volatile bool exiting;
const char *argp_program_version = "appnoise 1.0";
const char argp_program_doc[] =
"Trace app noise.\n"
"\n"
"USAGE: appnoise [-h] [-t TID] [-p PID] \n"
"\n"
"EXAMPLES:\n"
"   appnoise -p 123         # trace pid 123\n"
"   appnoise -t 123         # trace tid 123 (use for threads only)\n"
"   appnoise -t 123 -T      # trace tid 123 and print only the top events (default 10)\n"
"   appnoise -p 123 1 10    # trace pid 123 and print 1 second summaries, 10 times\n";


static const struct argp_option opts[] = {
	{ "pid", 'p', "PID", 0, "Process PID to trace"},
	{ "tid", 't', "TID", 0, "Thread TID to trace"},
	{ "top", 'T', "TOP", 0, "Print only the top events (default 10)" },
	{ NULL, 'h', NULL, OPTION_HIDDEN, "Show the full help" },
	{},
};
enum {
    OT_HIST = 0,
    OT_WAIT = 1,
    OT_IRQ = 2,
    OT_SOFTIRQ = 3,
    OT_SYSCALL = 4,
    OT_NUMA = 5,
    OT_NR,
};

enum {
	HI_SOFTIRQ = 0,
	TIMER_SOFTIRQ = 1,
	NET_TX_SOFTIRQ = 2,
	NET_RX_SOFTIRQ = 3,
	BLOCK_SOFTIRQ = 4,
	IRQ_POLL_SOFTIRQ = 5,
	TASKLET_SOFTIRQ = 6,
	SCHED_SOFTIRQ = 7,
	HRTIMER_SOFTIRQ = 8,
	RCU_SOFTIRQ = 9,
	NR_SOFTIRQS = 10,
};

static char *hist_names[] = {
	[hist_irq] = "irq",
	[hist_softirq] = "softirq",
	[hist_nmi] = "nmi",
	[hist_wait] = "wait",
	[hist_sleep] = "sleep",
	[hist_block] = "block",
	[hist_iowait] = "iowait",	
    [hist_syscall] = "syscall",
};

static char *hist_units[] = {
	[hist_irq] = "us",
	[hist_softirq] = "us",
	[hist_nmi] = "us",
	[hist_wait] = "us",
	[hist_sleep] = "s",
	[hist_block] = "s",
	[hist_iowait] = "s",	
    [hist_syscall] = "us",
};

static char *vec_names[] = {
	[HI_SOFTIRQ] = "hi",
	[TIMER_SOFTIRQ] = "timer",
	[NET_TX_SOFTIRQ] = "net_tx",
	[NET_RX_SOFTIRQ] = "net_rx",
	[BLOCK_SOFTIRQ] = "block",
	[IRQ_POLL_SOFTIRQ] = "irq_poll",
	[TASKLET_SOFTIRQ] = "tasklet",
	[SCHED_SOFTIRQ] = "sched",
	[HRTIMER_SOFTIRQ] = "hrtimer",
	[RCU_SOFTIRQ] = "rcu",
};

struct env{
    pid_t pid;
    pid_t tid;
    time_t interval;
    int times;
    int top;
	bool verbose;
} env = {
	.interval = 99999999,
	.times = 99999999,
	.top = 10,
	.verbose = false,
};

struct numa_info{
    __u64 count;
    char name[16];
    pid_t pid;
};

struct syscall_info{
    long long count;
    long long total_time;
    __u32 id;
    char name[32];
};

struct softirq_info{
    int vec;
    __u64 count;
    __u64 total_time;
    char name[32];
};

static bool (*print_func[OT_NR])(int);
static int map[OT_NR];

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
	if (env.verbose)
		return vfprintf(stderr, format, args);
	else
		return 0;
}

int check_and_fix_autoload(struct appnoise_bpf *obj)
{
	int i, ret = 0;
	char *str, *endptr;
	unsigned long ver[3];
	struct utsname ut;

	ret = uname(&ut);
	if (ret < 0)
		return -errno;

	str = ut.release;
	for (i = 0; i < 3; i++) {
		ver[i] = strtoul(str, &endptr, 10);
		if ((errno == ERANGE && (ver[i] == LONG_MAX || ver[i] == LONG_MIN))
			|| (errno != 0 && ver[i] == 0)) {
			perror("strtol");
			return -errno;
		}
		errno = 0;
		str = endptr+1;
	}

	if (ver[0] < 4 || ver[1] < 19) {
		bpf_program__set_autoload(obj->progs.handler_nmi, false);
		bpf_program__set_autoload(obj->progs.handler_sched_stat_wait, false);
		bpf_program__set_autoload(obj->progs.handler_sched_stat_sleep, false);
		bpf_program__set_autoload(obj->progs.handler_sched_stat_blocked, false);
		bpf_program__set_autoload(obj->progs.handler_sched_stat_iowait, false);
	}

	return 0;
}

void handle_event(void *ctx, int cpu, void *data, __u32 data_sz);

#define min(x, y) ({				\
	typeof(x) _min1 = (x);			\
	typeof(y) _min2 = (y);			\
	(void) (&_min1 == &_min2);		\
	_min1 < _min2 ? _min1 : _min2; })

static int compar(const void *dx , const void *dy)
{
    __u64 x = ((struct info *)dx)->total_time;
    __u64 y = ((struct info *)dy)->total_time;
    return x>y?-1:!(x==y);
}

static int compar_(const void *dx , const void *dy)
{
    __u64 x = ((struct thread_info_t *)dx)->count;
    __u64 y = ((struct thread_info_t *)dy)->count;
    return x>y?-1:!(x==y);
}

static void print_stars(unsigned int val, unsigned int val_max, int width)
{
	int num_stars, num_spaces, i;
	bool need_plus;

	num_stars = min(val, val_max) * width / val_max;
	num_spaces = width - num_stars;
	need_plus = val > val_max;

	for (i = 0; i < num_stars; i++)
		printf("*");
	for (i = 0; i < num_spaces; i++)
		printf(" ");
	if (need_plus)
		printf("+");
}

void print_log2_hist(unsigned int *vals, int vals_size, const char *val_type)
{
	int stars_max = 40, idx_max = -1;
	unsigned int val, val_max = 0;
	unsigned long long low, high;
	int stars, width, i;

	for (i = 0; i < vals_size; i++) {
		val = vals[i];
		if (val > 0)
			idx_max = i;
		if (val > val_max)
			val_max = val;
	}

	if (idx_max < 0)
		return;

	printf("%*s%-*s : count    distribution\n", idx_max <= 32 ? 10 : 20, "",
		idx_max <= 32 ? 14 : 24, val_type);

	if (idx_max <= 32)
		stars = stars_max;
	else
		stars = stars_max / 2;

	for (i = 0; i <= idx_max; i++) {
		low = (1ULL << (i + 1)) >> 1;
		high = (1ULL << (i + 1)) - 1;
		if (low == high)
			low -= 1;
		val = vals[i];
		width = idx_max <= 32 ? 10 : 20;
        if(i == idx_max && idx_max >= (MAX_SLOTS-1))
            printf("%*lld <= %-*s : %-8d |", width, low, width, "", val);
        else
		    printf("%*lld -> %-*lld : %-8d |", width, low, width, high, val);

		print_stars(val, val_max, stars);
		printf("|\n");
	}
}

static error_t parse_arg(int key,char *arg,struct argp_state *state)
{
    static int pos_args;
    int pid,top;
    switch (key){
        case 'h':
            argp_state_help(state,stderr,ARGP_HELP_STD_HELP);
            break;
        case 'p':
            errno = 0;
            pid = strtol(arg, NULL, 10);
            if (errno || pid <= 0) {
                fprintf(stderr, "Invalid PID: %s\n", arg);
                argp_usage(state);
            }
            env.pid = pid;
            break;
        case 't':
            errno = 0;
            pid = strtol(arg, NULL, 10);
            if (errno || pid <= 0) {
                fprintf(stderr, "Invalid TID: %s\n", arg);
                argp_usage(state);
            }
            env.tid = pid;
            break;
        case 'T':
            errno = 0;
            top = strtol(arg, NULL, 10);
            if (errno || top <= 0) {
                fprintf(stderr, "Invalid PID: %s\n", arg);
                argp_usage(state);
            }
            env.top = top;
            break;
        case ARGP_KEY_ARG:
            errno = 0;
            if(pos_args == 0){
                env.interval = strtol(arg,NULL,10);
                if(errno)
                {
                    fprintf(stderr,"invalid interval\n");
                    argp_usage(state);
                }
            }else if (pos_args == 1)
            {
                env.times = strtol(arg,NULL,10);
                if(errno)
                {
                    fprintf(stderr,"invalid times\n");
                }
            } else{
                fprintf(stderr,
                    "unrecognized positional argument: %s\n", arg);
                argp_usage(state);
            }
            pos_args++;
            break;
        case ARGP_KEY_END:
            if(!env.pid && !env.tid)
                {
                    printf("please input pid or tid to trace!\n");
                    return -1;
                }
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static bool print_hist_(int hist_map_id, int i)
{
    struct histinfo histinfo= {}, zero = {};
    int err;
    err = bpf_map_lookup_elem(hist_map_id,&i,&histinfo);
    if (err && errno != ENOENT) {
        printf("failed to lookup element: %s\n", strerror(errno));
        return false;
    }

    if(!histinfo.total_time)
        return true;
    printf("%s\n",hist_names[i]);
    print_log2_hist(histinfo.slots,MAX_SLOTS,hist_units[i]);
    printf("%*stotal%-*s: %-8lld(%s)\n", 10, "", 10, "", histinfo.total_time,hist_units[i]);
    printf("\n");
    err = bpf_map_update_elem(hist_map_id,&i,&zero,0);
    if(err)
    {
        printf("failed to clean histmap,err = %d\n",err);
        return false;
    }
	return true;
}

static bool print_hist (int fd)
{
    int i;
    bool res = true;
    for(i = 0 ; i < nr_hist;i++)
    {
        res = print_hist_(fd,i);
        if(!res)
        {
            printf("output hist err!\n");
            return false;
        }
    }
    return true;
}

static bool print_wait(int fd)
{
    int j, i = 0, err;
    pid_t pre = -1, key, keys[MAX_ENTRIES];
    struct thread_info_t values[MAX_ENTRIES],info;

    for(pre = -1;i < MAX_ENTRIES;)
    {
        err = bpf_map_get_next_key(fd,&pre,&key);
		if (err && errno != ENOENT) {
			printf("failed to get next key: %s\n", strerror(errno));
			return false;
		} else if (err) {
			break;
		}
        pre = keys[i++] = key;
    }

    if(i == 0)
        return true;

    for(j = 0; j < i ; j++)
    {
        err = bpf_map_lookup_elem(fd,&keys[j],&info);
        if (err && errno != ENOENT) {
			printf("failed to lookup element: %s\n", strerror(errno));
			return false;
		}
        values[j].count = info.count;
        values[j].pid = info.pid;
        strcpy(values[j].name,info.name);
    }

    for(j = 0 ; j < i ; j++)
    {
		err = bpf_map_delete_elem(fd, &keys[j]);
		if (err) {
			printf("failed to delete element: err = %d\n", err);
			return false;
		}
    }

    qsort(values,i,sizeof(values[0]),compar_);

    printf("%-20s %8s %8s\n","<TASK> -> CURR","PID","COUNT");
    for(j = 0 ; j < i ;j++)
    {
        printf("%-20s %8d %8lld\n",values[j].name,values[j].pid,values[j].count);
    }
    printf("\n");
    return true;
}

static bool print_irq(int fd)
{
    int j, err, i = 0, keys[MAX_ENTRIES_IRQ];
    unsigned int pre = -1, key;
    struct irq_info_t infos[MAX_ENTRIES_IRQ];

    for(pre = -1; i < MAX_ENTRIES_IRQ;)
    {
        err = bpf_map_get_next_key(fd,&pre,&key);
		if (err && errno != ENOENT) {
			printf("failed to get next key: %s\n", strerror(errno));
			return false;
		} else if (err) {
			break;
		}
        pre = keys[i++] = key;
    }

    if (i == 0)
        return true;

    for(j = 0; j < i ;j++)
    {
        err = bpf_map_lookup_elem(fd,&keys[j],&infos[j]);
        if (err && errno != ENOENT) {
			printf("failed to lookup element: %s\n", strerror(errno));
			return false;
		}
    }
    for(j = 0; j < i; j++) {
		err = bpf_map_delete_elem(fd, &keys[j]);
		if (err) {
			printf("failed to delete element: err = %d\n", err);
			return false;
		}
	}
    qsort(infos,i,sizeof(infos[0]),compar);

    printf("%-20s %-6s %-s(%s)\n","IRQ","COUNT","TOTAL",hist_units[hist_irq]);
    for(j = 0 ; j < i && j < env.top ;j++)
    {
            printf("%-20s %-6llu %-10llu\n",infos[j].name,infos[j].count,infos[j].total_time);
    }
    printf("\n");
    return true;
}

static bool print_softirq(int fd)
{
    struct info info , zero = {};
    struct softirq_info infos[NR_SOFTIRQS];
    __u64 total = 0;
    
    int err,vec;
    
    for(vec = 0 ; vec < NR_SOFTIRQS ; vec++)
    {
        err = bpf_map_lookup_elem(fd,&vec,&info);
		if (err < 0) {
			printf("failed to lookup infos: %d\n", err);
			return false;
		}
        infos[vec].vec = vec;
        infos[vec].total_time = info.total_time;
        infos[vec].count = info.count;
        strcpy(infos[vec].name,vec_names[vec]);
        total += info.total_time;
    }

    if(total == 0)
        return true;
    
    for(vec = 0 ; vec < NR_SOFTIRQS ; vec++)
    {
        err = bpf_map_update_elem(fd,&vec,&zero,0);
        if(err)
            {
                printf("softirq histmap clean failed.\n");
                return false;
            }
    }

    printf("%-10s %-6s %-s(%s) \n","SOFTIRQ","COUNT","TOTAL",hist_units[hist_irq]);
    for(vec = 0 ; vec < NR_SOFTIRQS;vec++)
    {
        if(infos[vec].count > 0)
            printf("%-10s %-6lld %-10lld\n",infos[vec].name,infos[vec].count,infos[vec].total_time);
    }
    printf("\n");
    return true;
}

static bool print_syscall(int fd)
{
    struct syscall_info infos[MAX_ENTRIES];
    int keys[MAX_ENTRIES];
    int j, i = 0,err;
    unsigned int pre = -1, key;
    struct info info;

    for(pre = -1; i < MAX_ENTRIES;)
    {
        err = bpf_map_get_next_key(fd,&pre,&key);
		if (err && errno != ENOENT) {
			printf("failed to get next key: %s\n", strerror(errno));
			return false;
		} else if (err) {
			break;
		}
        pre = keys[i++] = key;
    }

    if (i == 0)
        return true;

    for(j = 0; j < i ;j++)
    {
        err = bpf_map_lookup_elem(fd,&keys[j],&info);
        if (err && errno != ENOENT) {
			printf("failed to lookup element: %s\n", strerror(errno));
			return false;
		}
        infos[j].count = info.count;
        infos[j].total_time = info.total_time;
        infos[j].id = keys[j];
        syscall_name(infos[j].id,infos[j].name,sizeof(infos[j].name));
    }

	for(j = 0; j < i; j++) {
		err = bpf_map_delete_elem(fd, &keys[j]);
		if (err) {
			printf("failed to delete element: err = %d\n", err);
			return false;
		}
	}

    qsort(infos, i , sizeof(infos[0]), compar);

    printf("%-22s %8s %8s(%s)\n","SYSCALL","COUNT","TOTAL",hist_units[hist_syscall]);
    for(j = 0 ; j < i && j < env.top ;j++)
    {
        printf("%-22s %8lld %8lld\n",infos[j].name,infos[j].count,infos[j].total_time);
    }
    return true;
}

static bool print_numa(int fd)
{
    pid_t pre = -1, key, keys[MAX_ENTRIES];
    struct thread_info_t values[MAX_ENTRIES],value;
    int j, i = 0,err;
    for(pre = -1;i<MAX_ENTRIES;)
    {
        err = bpf_map_get_next_key(fd,&pre,&key);
		if (err && errno != ENOENT) {
			printf("failed to get next key: %s\n", strerror(errno));
			return false;
		} else if (err) {
			break;
		}
        pre = keys[i++] = key;
    }

    if (i == 0)
        return true;
    
    for(j = 0 ; j < i ; j++)
    {
        err = bpf_map_lookup_elem(fd,&keys[j],&value);
        if (err && errno != ENOENT) {
			printf("failed to lookup element: %s\n", strerror(errno));
			return false;
		}
        values[j].count = value.count;
        values[j].pid = value.pid;
        strcpy(values[j].name,value.name);
    }
    
    for(j = 0 ; j < i ; j ++)
    {
        err = bpf_map_delete_elem(fd,&keys[j]);
        if (err) {
			printf("failed to delete element: err = %d\n", err);
			return false;
		}
    }

    qsort(values,i,sizeof(values[0]),compar_);

    printf("Cross-NUMA Memory Access Info\n%-22s %8s %8s\n","NAME","PID","COUNT");
    for(j = 0 ; j < i ; j++)
    {
        printf("%-22s %8d %8lld\n",values[j].name,values[j].pid,values[j].count);
    }
    printf("\n");
    return true;
}

static void sig_handler(int sig)
{
	exiting = true;
}

static void init_output(struct appnoise_bpf *obj)
{
    print_func[OT_HIST] = print_hist;
    print_func[OT_WAIT] = print_wait;
    print_func[OT_IRQ] = print_irq;
    print_func[OT_SOFTIRQ] = print_softirq;
    print_func[OT_NUMA] = print_numa;
    print_func[OT_SYSCALL] = print_syscall;

    map[OT_HIST] = bpf_map__fd(obj->maps.histmap);
    map[OT_IRQ] = bpf_map__fd(obj->maps.irq_infos);
    map[OT_SOFTIRQ] =  bpf_map__fd(obj->maps.softirq_infos);
    map[OT_SYSCALL] = bpf_map__fd(obj->maps.syscall_infos);
    map[OT_NUMA] = bpf_map__fd(obj->maps.numa_map);
    map[OT_WAIT] = bpf_map__fd(obj->maps.wait_thread_info);
}

static void bump_memlock_rlimit(void)
{
	struct rlimit rlim_new = {
		.rlim_cur	= RLIM_INFINITY,
		.rlim_max	= RLIM_INFINITY,
	};

	if (setrlimit(RLIMIT_MEMLOCK, &rlim_new)) {
		fprintf(stderr, "Failed to increase RLIMIT_MEMLOCK limit!\n");
		exit(1);
	}
}

int main(int argc, char *argv[])
{
    static const struct argp argp = {
        .options = opts,
        .parser = parse_arg,
        .doc = argp_program_doc,
    };

    int err,map_fd,i = 0;
    struct appnoise_bpf *obj;
    struct args args = {};
	struct tm *tm;
	char ts[32];
	time_t t;

    init_syscall_names();

    err = argp_parse(&argp,argc,argv,0,NULL,NULL);
    if(err)
        return err;

	libbpf_set_print(libbpf_print_fn);
    bump_memlock_rlimit();
    obj = appnoise_bpf__open();
    if(!obj)
    {
        printf("failed to open BPF object\n");
        return 1;
    }
	check_and_fix_autoload(obj);
    err = appnoise_bpf__load(obj);
    if(err)
    {
        printf("failed to load BPF object: %d\n",err);
        goto cleanup;
    }

    map_fd = bpf_map__fd(obj->maps.filter_map);
    args.pid = env.tid;
    args.tgid = env.pid;
    err = bpf_map_update_elem(map_fd,&i,&args,0);

    if(err)
    {
        printf("failed to update args map\n");
        goto cleanup;
    }

    err = appnoise_bpf__attach(obj);
    if(err)
    {
        printf("failed to attach BPF programs\n");
        goto cleanup;
    }

    signal(SIGINT, sig_handler);

    printf("Tracing... Hit Ctrl-C to end.\n");

    init_output(obj);

    while(1){
	int i;
        bool res = true;
        sleep(env.interval);
        
        time(&t);
        tm = localtime(&t);
        strftime(ts, sizeof(ts), "%H:%M:%S", tm);
        printf("%-8s\n", ts);

        for(i = 0 ; i < OT_NR;i++)
        {
            res = print_func[i](map[i]);
            if(!res)
            {
                printf("output erro!\n");
                break;
            }
        }

        printf("\n");
		if (exiting || --env.times == 0)
			break;
    }

cleanup:
    appnoise_bpf__destroy(obj);
    free_syscall_names();
    return 0;
}
