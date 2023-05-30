// SPDX-License-Identifier: MIT

/* Check available memory and swap in a loop and start killing
 * processes if they get too low */

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#include "globals.h"
#include "kill.h"
#include "meminfo.h"
#include "msg.h"

/* Don't fail compilation if the user has an old glibc that
 * does not define MCL_ONFAULT. The kernel may still be recent
 * enough to support the flag.
 */
#ifndef MCL_ONFAULT
#define MCL_ONFAULT 4
#endif

#ifndef VERSION
#define VERSION "*** v1.0 version ***"
#endif

/* Arbitrary identifiers for long options that do not have a short
 * version */
enum {
    LONG_OPT_PREFER = 513,
    LONG_OPT_AVOID,
    LONG_OPT_DRYRUN,
    LONG_OPT_IGNORE,
    LONG_OPT_IGNORE_ROOT,
};

static int set_oom_score_adj(int);
static void poll_loop(poll_loop_args_t* args);
extern int metric_init(poll_loop_args_t *poll);
extern int get_cpu_stat(poll_loop_args_t *poll);
extern int metric_exit(poll_loop_args_t *poll);
extern int event_poll(poll_loop_args_t *poll, int timeout);
// Prevent Golang / Cgo name collision when the test suite runs -
// Cgo generates it's own main function.
#ifdef CGO
#define main main2
#endif

double min(double x, double y)
{
    if (x < y)
        return x;
    return y;
}

// Dry-run oom kill to make sure that
// (1) it works (meaning /proc is accessible)
// (2) the stack grows to maximum size before calling mlockall()
static void startup_selftests(poll_loop_args_t* args)
{
    {
        debug("%s: dry-running oom kill...\n", __func__);
        procinfo_t victim = find_largest_process(args);
        kill_process(args, 0, &victim);
    }
    if (args->notify_ext) {
        if (args->notify_ext[0] != '/') {
            warn("%s: -N: notify script '%s' is not an absolute path, disabling -N\n", __func__, args->notify_ext);
            args->notify_ext = NULL;
        } else if (access(args->notify_ext, X_OK)) {
            warn("%s: -N: notify script '%s' is not executable: %s\n", __func__, args->notify_ext, strerror(errno));
        }
    }
}

int main(int argc, char* argv[])
{
    poll_loop_args_t args = {
        .mem_term_percent = 7,
        .swap_term_percent = 10,
        .mem_kill_percent = 5,
        .swap_kill_percent = 5,
        .report_interval_ms = 10000,
        .iowait_thres = 30,
        .sys_thres = 40,
        .ignore_root_user = false,
        .kill_mode = KILL_MODE_1,
        /* omitted fields are set to zero */
    };
    int set_my_priority = 1;
    char* prefer_cmds = NULL;
    char* avoid_cmds = NULL;
    char* ignore_cmds = NULL;
    regex_t _prefer_regex;
    regex_t _avoid_regex;
    regex_t _ignore_regex;

    /* request line buffering for stdout - otherwise the output
     * may lag behind stderr */
    setlinebuf(stdout);

    /* clean up dbus-send zombies */
    signal(SIGCHLD, SIG_IGN);

    fprintf(stderr, "oomkill " VERSION "\n");

    if (chdir(procdir_path) != 0) {
        fatal(4, "Could not cd to /proc: %s", strerror(errno));
    }

    // PR_CAP_AMBIENT is not available on kernel < 4.3
#ifdef PR_CAP_AMBIENT
    // When systemd starts a daemon with capabilities, it uses ambient
    // capabilities to do so. If not dropped, the capabilities can spread
    // to any child process. This is usually not necessary and its a good
    // idea to drop them if not needed.
    prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL, 0, 0, 0);
#endif

    meminfo_t m = parse_meminfo();

    int c;
    const char* short_opt = "m:k:i:I:f:s:M:S:ngN:dvr:ph";
    struct option long_opt[] = {
        { "prefer", required_argument, NULL, LONG_OPT_PREFER },
        { "avoid", required_argument, NULL, LONG_OPT_AVOID },
        { "ignore", required_argument, NULL, LONG_OPT_IGNORE },
        { "dryrun", no_argument, NULL, LONG_OPT_DRYRUN },
        { "ignore-root-user", no_argument, NULL, LONG_OPT_IGNORE_ROOT },
        { "help", no_argument, NULL, 'h' },
        { 0, 0, NULL, 0 } /* end-of-array marker */
    };
    bool have_m = 0, have_M = 0, have_s = 0, have_S = 0;
    double mem_term_kib = 0, mem_kill_kib = 0, swap_term_kib = 0, swap_kill_kib = 0;

    while ((c = getopt_long(argc, argv, short_opt, long_opt, NULL)) != -1) {
        float report_interval_f = 0;
        term_kill_tuple_t tuple;

        switch (c) {
        case -1: /* no more arguments */
        case 0: /* long option toggles */
            break;
        case 'm':
            // Use 99 as upper limit. Passing "-m 100" makes no sense.
            tuple = parse_term_kill_tuple(optarg, 99);
            if (strlen(tuple.err)) {
                fatal(15, "-m: %s", tuple.err);
            }
            args.mem_term_percent = tuple.term;
            args.mem_kill_percent = tuple.kill;
            have_m = 1;
            break;
        case 's':
            // Using "-s 100" is a valid way to ignore swap usage
            tuple = parse_term_kill_tuple(optarg, 100);
            if (strlen(tuple.err)) {
                fatal(16, "-s: %s", tuple.err);
            }
            args.swap_term_percent = tuple.term;
            args.swap_kill_percent = tuple.kill;
            have_s = 1;
            break;
        case 'M':
            tuple = parse_term_kill_tuple(optarg, m.MemTotalKiB * 100 / 99);
            if (strlen(tuple.err)) {
                fatal(15, "-M: %s", tuple.err);
            }
            mem_term_kib = tuple.term;
            mem_kill_kib = tuple.kill;
            have_M = 1;
            break;
        case 'k':
           args.kill_mode = strtol(optarg, NULL, 10);
           printf("kill mode:%d\n", args.kill_mode);
           break;
        case 'S':
            tuple = parse_term_kill_tuple(optarg, m.SwapTotalKiB * 100 / 99);
            if (strlen(tuple.err)) {
                fatal(16, "-S: %s", tuple.err);
            }
            if (m.SwapTotalKiB == 0) {
                warn("warning: -S: total swap is zero, using default percentages\n");
                break;
            }
            swap_term_kib = tuple.term;
            break;
        case 'i':
            args.iowait_thres = strtol(optarg, NULL, 10);
            printf("iowait_thres:%ld\n", args.iowait_thres);
            break;
        case 'I':
            args.sys_thres = strtol(optarg, NULL, 10);
            printf("sys_thres:%ld\n", args.sys_thres);
            break;
        case 'n':
            args.notify = true;
            fprintf(stderr, "Notifying through D-Bus\n");
            break;
        case 'g':
            args.kill_process_group = true;
            break;
        case 'N':
            args.notify_ext = optarg;
            break;
        case 'd':
            enable_debug = 1;
            break;
        case 'v':
            // The version has already been printed above
            exit(0);
        case 'r':
            report_interval_f = strtof(optarg, NULL);
            if (report_interval_f >= 1) {
                args.report_interval_ms = (int)(report_interval_f * 1000);
            } else {
                warn("-r: invalid interval '%s' ,need to > 1s\n", optarg);
            }
            
            break;
        case 'p':
            set_my_priority = 1;
            break;
        case LONG_OPT_IGNORE_ROOT:
            args.ignore_root_user = true;
            fprintf(stderr, "Processes owned by root will not be killed\n");
            break;
        case LONG_OPT_PREFER:
            prefer_cmds = optarg;
            break;
        case LONG_OPT_AVOID:
            avoid_cmds = optarg;
            break;
        case LONG_OPT_DRYRUN:
            warn("dryrun mode enabled, will not kill anything\n");
            args.dryrun = 1;
            break;
        case LONG_OPT_IGNORE:
            ignore_cmds = optarg;
            break;
        case 'h':
            fprintf(stderr,
                "Usage: %s [OPTION]...\n"
                "\n"
                "  -m PERCENT[,KILL_PERCENT] set available memory minimum to PERCENT of total\n"
                "                            (default 10 %%).\n"
                "                            oomkill sends SIGTERM once below PERCENT, then\n"
                "                            SIGKILL once below KILL_PERCENT (default PERCENT/2).\n"
                "  -s PERCENT[,KILL_PERCENT] set free swap minimum to PERCENT of total (default\n"
                "                            10 %%).\n"
                "                            Note: both memory and swap must be below minimum for\n"
                "                            oomkill to act.\n"
                "  -M SIZE[,KILL_SIZE]       set available memory minimum to SIZE KiB\n"
                "  -S SIZE[,KILL_SIZE]       set free swap minimum to SIZE KiB\n"
                "  -k                        kill mode: 0, 1, 2, 3 (default 1)\n"
                "  -i                        cpu iowait value (default 30)\n"
                "  -I                        cpu system value (default 30)\n"
                "  -n                        enable d-bus notifications\n"
                "  -N /PATH/TO/SCRIPT        call script after oom kill\n"
                "  -g                        kill all processes within a process group\n"
                "  -d                        enable debugging messages\n"
                "  -v                        print version information and exit\n"
                "  -r INTERVAL               memory report interval in seconds (default 1), set\n"
                "                            to 0 to disable completely\n"
                "  -p                        set niceness of oomkill to -20 and oom_score_adj to\n"
                "                            -100\n"
                "  --ignore-root-user        do not kill processes owned by root\n"
                "  --prefer REGEX            prefer to kill processes matching REGEX\n"
                "  --avoid REGEX             avoid killing processes matching REGEX\n"
                "  --ignore REGEX            ignore processes matching REGEX\n"
                "  --dryrun                  dry run (do not kill any processes)\n"
                "  -h, --help                this help text\n",
                argv[0]);
            exit(0);
        case '?':
            fprintf(stderr, "Try 'oomkill --help' for more information.\n");
            exit(13);
        }
    } /* while getopt */

    // Merge "-M" with "-m" values
    if (have_M) {
        double M_term_percent = 100 * mem_term_kib / (double)m.MemTotalKiB;
        double M_kill_percent = 100 * mem_kill_kib / (double)m.MemTotalKiB;
        if (have_m) {
            // Both -m and -M were passed. Use the lower of both values.
            args.mem_term_percent = min(args.mem_term_percent, M_term_percent);
            args.mem_kill_percent = min(args.mem_kill_percent, M_kill_percent);
        } else {
            // Only -M was passed.
            args.mem_term_percent = M_term_percent;
            args.mem_kill_percent = M_kill_percent;
        }
    }
    // Merge "-S" with "-s" values
    if (have_S) {
        double S_term_percent = 100 * swap_term_kib / (double)m.SwapTotalKiB;
        double S_kill_percent = 100 * swap_kill_kib / (double)m.SwapTotalKiB;
        if (have_s) {
            // Both -s and -S were passed. Use the lower of both values.
            args.swap_term_percent = min(args.swap_term_percent, S_term_percent);
            args.swap_kill_percent = min(args.swap_kill_percent, S_kill_percent);
        } else {
            // Only -S was passed.
            args.swap_term_percent = S_term_percent;
            args.swap_kill_percent = S_kill_percent;
        }
    }
    if (prefer_cmds) {
        args.prefer_regex = &_prefer_regex;
        if (regcomp(args.prefer_regex, prefer_cmds, REG_EXTENDED | REG_NOSUB) != 0) {
            fatal(6, "could not compile regexp '%s'\n", prefer_cmds);
        }
        fprintf(stderr, "Preferring to kill process names that match regex '%s'\n", prefer_cmds);
    }
    if (avoid_cmds) {
        args.avoid_regex = &_avoid_regex;
        if (regcomp(args.avoid_regex, avoid_cmds, REG_EXTENDED | REG_NOSUB) != 0) {
            fatal(6, "could not compile regexp '%s'\n", avoid_cmds);
        }
        fprintf(stderr, "Will avoid killing process names that match regex '%s'\n", avoid_cmds);
    }
    if (ignore_cmds) {
        args.ignore_regex = &_ignore_regex;
        if (regcomp(args.ignore_regex, ignore_cmds, REG_EXTENDED | REG_NOSUB) != 0) {
            fatal(6, "could not compile regexp '%s'\n", ignore_cmds);
        }
        fprintf(stderr, "Will ignore process names that match regex '%s'\n", ignore_cmds);
    }
    if (set_my_priority) {
        bool fail = 0;
        if (setpriority(PRIO_PROCESS, 0, -20) != 0) {
            warn("Could not set priority: %s. Continuing anyway\n", strerror(errno));
            fail = 1;
        }
        int ret = set_oom_score_adj(-100);
        if (ret != 0) {
            warn("Could not set oom_score_adj: %s. Continuing anyway\n", strerror(ret));
            fail = 1;
        }
        if (!fail) {
            fprintf(stderr, "Priority was raised successfully\n");
        }
    }

    startup_selftests(&args);

    int err = mlockall(MCL_CURRENT | MCL_FUTURE | MCL_ONFAULT);
    // kernels older than 4.4 don't support MCL_ONFAULT. Retry without it.
    if (err != 0) {
        err = mlockall(MCL_CURRENT | MCL_FUTURE);
    }
    if (err != 0) {
        perror("Could not lock memory - continuing anyway");
    }
    
    metric_init(&args);
    // Jump into main poll loop
    poll_loop(&args);

    metric_exit(&args);
    return 0;
}

// Returns errno (success = 0)
static int set_oom_score_adj(int oom_score_adj)
{
    char buf[PATH_LEN] = { 0 };
    pid_t pid = getpid();

    snprintf(buf, sizeof(buf), "%s/%d/oom_score_adj", procdir_path, pid);
    FILE* f = fopen(buf, "w");
    if (f == NULL) {
        return -1;
    }

    // fprintf returns a negative error code on failure
    int ret1 = fprintf(f, "%d", oom_score_adj);
    // fclose returns a non-zero value on failure and errno contains the error code
    int ret2 = fclose(f);

    if (ret1 < 0) {
        return -ret1;
    }
    if (ret2) {
        return errno;
    }
    return 0;
}

/* Calculate the time we should sleep based upon how far away from the memory and swap
 * limits we are (headroom). Returns a millisecond value between 100 and 1000 (inclusive).
 * The idea is simple: if memory and swap can only fill up so fast, we know how long we can sleep
 * without risking to miss a low memory event.
 */
static unsigned sleep_time_ms(const poll_loop_args_t* args, const meminfo_t* m)
{
    // Maximum expected memory/swap fill rate. In kiB per millisecond ==~ MiB per second.
    const long long mem_fill_rate = 6000; // 6000MiB/s seen with "stress -m 4 --vm-bytes 4G"
    const long long swap_fill_rate = 800; //  800MiB/s seen with membomb on ZRAM
    // Clamp calculated value to this range (milliseconds)
    const unsigned min_sleep = 100;
    const unsigned max_sleep = 1000;

    long long mem_headroom_kib = (long long)((m->MemAvailablePercent - args->mem_term_percent) * (double)m->UserMemTotalKiB / 100);
    if (mem_headroom_kib < 0) {
        mem_headroom_kib = 0;
    }
    long long swap_headroom_kib = (long long)((m->SwapFreePercent - args->swap_term_percent) * (double)m->SwapTotalKiB / 100);
    if (swap_headroom_kib < 0) {
        swap_headroom_kib = 0;
    }
    long long ms = mem_headroom_kib / mem_fill_rate + swap_headroom_kib / swap_fill_rate;
    if (ms < min_sleep) {
        return min_sleep;
    }
    if (ms > max_sleep) {
        return max_sleep;
    }
    return (unsigned)ms;
}

/* lowmem_sig compares the limits with the current memory situation
 * and returns which signal (SIGKILL, SIGTERM, 0) should be sent in
 * response. 0 means that there is enough memory and we should
 * not kill anything.
 */

static int get_kill_args(const poll_loop_args_t* args, struct kill_args *thres, int fast)
{
	long iowait_avg = 0;
	long iowait_thres = 0;
	long sys_thres = 0;
	long sys_avg = 0;
	int ret = 0;
    int kill_mode = 0;

    kill_mode = args->kill_mode;
    switch (kill_mode) {
        case KILL_MODE_0:
            memset(thres, 0, sizeof(*thres));
            break;
        case KILL_MODE_2:
            iowait_avg = args->cstat_util.iowait_avg30;
            sys_avg = args->cstat_util.system_avg30;
            break;
        case KILL_MODE_3:
            iowait_avg = args->cstat_util.iowait_avg60;
            sys_avg = args->cstat_util.system_avg60;
            break;
        default:
            iowait_avg = args->cstat_util.iowait_avg10;
            sys_avg = args->cstat_util.system_avg10;
    }
	
    if (fast) {
	   iowait_thres = args->iowait_thres - 10;
	   sys_thres = args->sys_thres - 10;
	} else {
	   iowait_thres = args->iowait_thres + 10;
	   sys_thres = args->sys_thres + 10;
	}

    thres->iowait_avg = iowait_avg;
    thres->iowait_thres = iowait_thres;
    thres->sys_avg = sys_avg;
    thres->sys_thres = sys_thres;
    thres->kill_mode = kill_mode;

	return ret;
}

static int high_iowait(const poll_loop_args_t* args, struct kill_args *thres)
{
    int ret = 0;

    if (thres->kill_mode == KILL_MODE_0)
        return (args->cstat_util.iowait > (args->iowait_thres - 15));

    else 
        return !!((thres->iowait_avg >= thres->iowait_thres) && (args->cstat_util.iowait > args->iowait_thres));
}

static int high_system(const poll_loop_args_t* args, struct kill_args *thres)
{
    int ret = 0;

    if (thres->kill_mode = KILL_MODE_0)
        return (args->cstat_util.system >= (args->sys_thres - 15));
    else
        return !!((thres->sys_avg >= thres->sys_thres) && (args->cstat_util.system >= args->sys_thres));
	return ret;
}

static int low_mem(const poll_loop_args_t* args, const meminfo_t* m, int fast, struct kill_mode_args *thres)
{
	long percent = 0;
	long size = 0;
	int ret = 0;
	if (fast) {
	   size = WARN_KSIZE;
	   percent = args->mem_term_percent;
	} else {
	   size = KILL_KSIZE;
	   percent = args->mem_kill_percent;
	}
	ret = !!((m->MemAvailablePercent <= percent) && (m->MemAvailableKiB <= size));
	return ret;
}

static int low_cache(const poll_loop_args_t* args, const meminfo_t* m)
{
	int ret ;
    
    if (args->kill_mode == KILL_MODE_0)
        return 1;

	ret = !!((m->MemFileCacheKiB <= m->MemTotalKiB*KILL_CACHE_RATE) && (m->MemFileCacheKiB<= KILL_CACHE_KSIZE));	
	return ret;
}

static int lowmem_sig(const poll_loop_args_t* args, const meminfo_t* m)
{
    struct kill_args thres;

    memset(&thres, 0, sizeof(thres));
    get_kill_args(args, &thres, 0);   
    if (low_mem(args,m, 0, &thres) && (high_iowait(args, &thres) || high_system(args, &thres)) && low_cache(args, m)) {
        return SIGKILL;
    }

    memset(&thres, 0, sizeof(thres));
    get_kill_args(args, &thres, 1);
    if (low_mem(args,m, 1, &thres) && (high_iowait(args, &thres) || high_system(args, &thres)) && low_cache(args, m)) {
        return SIGTERM;
    }

    return 0;
}
/* enter warning mode for MemAvailable < MemTotal*10% && MemAvailable < 6.4G 
 *
 *
 */
static int mem_status(poll_loop_args_t* args, const meminfo_t* m)
{
    int mode = args->mode;
    if ((m->MemAvailableKiB < WARN_KSIZE) && (m->MemAvailableKiB <= (m->MemTotalKiB * WARN_RATE))) {
        mode = WARN;
    }
    if ((m->MemAvailableKiB > NOR_KSIZE) || ((m->MemAvailableKiB > m->MemTotalKiB * NOR_RATE))) {
        mode = NORMAL;
    }
    return mode; 
}

void print_iowait(poll_loop_args_t *poll)
{
	warn("iowait: %.2f%% iowaitavg10: %2.f%% iowaitavg30: %2.f%% iowaitavg60: %2.f%%\n", \
        poll->cstat_util.iowait, poll->cstat_util.iowait_avg10, \
        poll->cstat_util.iowait_avg30, poll->cstat_util.iowait_avg60);
}

void print_system(poll_loop_args_t *poll)
{
	warn("system: %.2f%% systemavg10: %2.f%% systemavg30: %2.f%% systemavg60: %2.f%%\n", \
        poll->cstat_util.system, poll->cstat_util.system_avg10, \
        poll->cstat_util.system_avg30, poll->cstat_util.system_avg60);
}


void print_killinfo(poll_loop_args_t *poll)
{
    meminfo_t m;
    m = parse_meminfo();
    print_mem_stats(warn, m);
    warn("min:%ld low: %ld  high: %ld\n", poll->min, poll->low, poll->high);
    print_iowait(poll);
    print_system(poll);
    warn("\n");
}


// poll_loop is the main event loop. Never returns.
static void poll_loop(poll_loop_args_t* args)
{
    // Print a a memory report when this reaches zero. We start at zero so
    // we print the first report immediately.
    int report_countdown_ms = 0;
    int report_prev_ms = args->report_interval_ms;
    struct timeval start, end;
    int ret = 0;

    printf("args->report_interval_ms:%d\n",args->report_interval_ms);
    args->mode = NORMAL;
    while (1) {
        gettimeofday(&start, NULL);
        meminfo_t m = parse_meminfo();
        int sig = 0;
        args->m = m;

        get_cpu_stat(args);
        mem_status(args, &m);
        if ((args->mode != WARN) && (mem_status(args, &m)== WARN)) {
            args->report_interval_ms = 1000;
            args->mode = WARN;
            warn("low Available memory entry warning mode \n");
        } else if ((args->mode != NORMAL) && (mem_status(args, &m)== NORMAL)) {
            warn("normal Available memory entry normal mode :%d \n", args->mode);
            args->report_interval_ms = report_prev_ms;
            args->mode = NORMAL;
        }

        sig = lowmem_sig(args, &m);
        if (sig == SIGKILL) {
            warn("low memory! at or below SIGKILL limits: mem " PRIPCT ", swap " PRIPCT "\n",
                args->mem_kill_percent, args->swap_kill_percent);
            //print_mem_stats(warn, m);
        } else if (sig == SIGTERM) {
            warn("low memory! at or below SIGTERM limits: mem " PRIPCT ", swap " PRIPCT "\n",
                args->mem_term_percent, args->swap_term_percent);
            //print_mem_stats(warn, m);
        }
        if (sig) {
            procinfo_t victim = find_largest_process(args);
            /* The run time of find_largest_process is proportional to the number
             * of processes, and takes 2.5ms on my box with a running Gnome desktop (try "make bench").
             * This is long enough that the situation may have changed in the meantime,
             * so we double-check if we still need to kill anything.
             * The run time of parse_meminfo is only 6us on my box and independent of the number
             * of processes (try "make bench").
             */
            m = parse_meminfo();
            args->m = m;
            if (lowmem_sig(args, &m) == 0) {
                warn("memory situation has recovered while selecting victim\n");
            } else {
                kill_process(args, sig, &victim);
                print_killinfo(args);
            }
        } else if (args->report_interval_ms && report_countdown_ms <= 0) {
	        print_mem_stats(warn, m);
	        print_iowait(args);
	        print_system(args);
            if (args->mode == NORMAL)
                report_countdown_ms = args->report_interval_ms * 6;
            else
                report_countdown_ms = args->report_interval_ms * 5;
        }
        gettimeofday(&end, NULL);
        long sleep_ms = (end.tv_sec - start.tv_sec)*1000 + (end.tv_usec - start.tv_usec)/1000;
        if (args->report_interval_ms > sleep_ms)
            sleep_ms = args->report_interval_ms - sleep_ms;
        else
            sleep_ms = args->report_interval_ms;

        struct timespec req = { .tv_sec = (time_t)(sleep_ms / 1000), .tv_nsec = (sleep_ms % 1000) * 1000000 };
        if (args->mode == WARN)
            nanosleep(&req, &req);
        else {
            ret = event_poll(args, sleep_ms);
            if (ret > 0) {
                args->mode = WARN;
                args->report_interval_ms = 1000;
                warn("event poll set to waring mode\n");
            }
        }
        report_countdown_ms -= (int)sleep_ms;
    }
}
