#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <hashmap.h>
#include <argp.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/utsname.h>

#include "process_tree.h"
#include "bpf/process_tree.skel.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Global Variables
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static struct env
{
    pid_t pid;
    char comm[TASK_COMM_LEN];
    bool debug;
    int duration;
    char *btf_custom_path;
    char args[MAX_ARR_LEN];
    bool loop_flag;
    struct hashmap *comms;
    int relations_map_fd;
    struct hashmap *argses;
} env = {
    .duration = 10,
    .debug = false,
    .btf_custom_path = NULL,
    .pid = 1,
    .loop_flag = true,
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Arguments parse
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char *argp_program_version = "0.1";
const char *argp_program_bug_address = "https://gitee.com/anolis/sysak";
static const char argp_program_doc[] =
    "\nTracks the process chain of a given comm\n"
    "\n"
    "EXAMPLES:\n"
    "   # The program runs for 10 seconds, during which the process chain of the curl command is traced\n"
    "   sysak process_tree -c curl -t 10    \n"
    "   ......                            \n";

static const struct argp_option process_tree_options[] = {
    // {"pid", 'p', "ROOT_PROCESS_ID", 0, "Root pid for process tree"},
    {"comm", 'c', "FILTER_COMM", 0, "Process's comm used to filter output result"},
    {"time", 't', "Running time", 0, "Duration of diagnosis"},
    {"btf", 'b', "BTF_PATH", 0, "Specify path of the custom btf"},
    {"debug", 'd', NULL, 0, "Enable libbpf debug output"},
    {NULL, 'h', NULL, OPTION_HIDDEN, "Show the full help"},
    {},
};

static error_t parse_arg(int key, char *arg, struct argp_state *state)
{
    int time;
    switch (key)
    {
    case 'd':
        env.debug = true;
        break;
    case 'b':
        env.btf_custom_path = arg;
        break;
    case 'c':
        if (strlen(arg) > TASK_COMM_LEN)
        {
            fprintf(stderr, "Invalid comm, length must < : %d\n", TASK_COMM_LEN);
        }
        else
        {
            strncpy(env.comm, arg, strlen(arg) + 1);
        }
        break;
    case 't':
        time = strtol(arg, NULL, 10);
        if (errno || time <= 0)
        {
            fprintf(stderr, "Invalid PID: %s\n", arg);
            argp_usage(state);
        }
        env.duration = time;
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helper functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static inline size_t int_hash_fn(const void *key, void *ctx)
{
    return *((int *)key);
}

static inline size_t int_hash_fn_equal(const void *key1, const void *key2, void *ctx)
{
    return *((int *)key1) == *((int *)key2);
}

static int libbpf_print_fn(enum libbpf_print_level level,
                           const char *format, va_list args)
{
    if (!env.debug)
        return 0;
    return vfprintf(stderr, format, args);
}

/**
 * Save args to env.args
 * Due to args disable to modified => will cause program coredump
 */
static void extract_process_args(char *dst, char *src, int maxLen)
{
    for (int i = 0; i < maxLen - 1; i++)
    {
        if (src[i] == '\0')
        {
            if (src[i + 1] != '\0')
            {
                dst[i] = ' ';
            }
            else
            {
                break;
            }
        }
        else
        {
            dst[i] = src[i];
        }
    }
    dst[MAX_ARR_LEN - 1] = '\0';
}

/**
 * Diable autoload, ensure we can use maps to pass args
 */
int check_and_fix_autoload(struct process_tree_bpf *obj)
{
    int i, ret = 0;
    char *str, *endptr;
    unsigned long ver[3];
    struct utsname ut;

    ret = uname(&ut);
    if (ret < 0)
        return -errno;

    str = ut.release;
    for (i = 0; i < 3; i++)
    {
        ver[i] = strtoul(str, &endptr, 10);
        if ((errno == ERANGE && (ver[i] == LONG_MAX || ver[i] == LONG_MIN)) || (errno != 0 && ver[i] == 0))
        {
            perror("strtol");
            return -errno;
        }
        errno = 0;
        str = endptr + 1;
    }

    if (ver[0] < 4 || ver[1] < 19)
    {
        bpf_program__set_autoload(obj->progs.trace_sched_process_fork, false);
        bpf_program__set_autoload(obj->progs.trace_shced_process_exec, false);
    }

    return 0;
}

/**
 * Print the process chain for the specified PID
 * @param pid Target pid
 */
void trace_back(int pid)
{
    int cur_pid = pid;
    int next_pid = 0;
    char *cur_comm;
    char *cur_args;
    int space_count = 0;
    do
    {
        hashmap__find(env.comms, &cur_pid, &cur_comm);
        hashmap__find(env.argses, &cur_pid, &cur_args);
        if (space_count == 0)
        {
            printf("=========================%d(%s)==========================\n", cur_pid, cur_comm);
        }
        else
        {
            printf("%*s↓\n", space_count, " ");
        }
        printf("%*s", space_count, " ");
        printf("%d(%s, %s)\n", cur_pid, cur_comm, cur_args);
        next_pid = -1;
        bpf_map_lookup_elem(env.relations_map_fd, &cur_pid, &next_pid);
        cur_pid = next_pid;
        space_count += 2;

    } while (next_pid >= 0);
    printf("\n");
}

/**
 * Alarm handler, used to stop program in specific seconds
 */
void signal_alarm_func(int sig)
{
    env.loop_flag = false;
    struct hashmap_entry *cur, *tmp;
    int i;

    int target_comm_length = strlen(env.comm);

    // Output diagnosis result
    hashmap__for_each_entry(env.comms, cur, i)
    {
        if (target_comm_length <= 0)
        {
            trace_back(*((int *)cur->key));
        }
        else if (strncmp((char *)cur->value, env.comm, target_comm_length) == 0)
        {
            trace_back(*((int *)cur->key));
        }
        // printf("pid: %d, comm: %s\n", *((int *)cur->key), (char *)cur->value);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// EBPF event handle
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Handle ebpf perf_event
 */
static void handle_event(void *ctx, int cpu, void *data, __u32 data_sz)
{
    struct process_tree_event *ep = (struct process_tree_event *)data;
    char *arg_ptr = &env.args[0];
    char *comm_ptr = NULL;
    bool find = false;
    int c_pid = ep->c_pid;
    switch (ep->type)
    {
    case 0:
        // printf("%d -> %d, %s, %s\n", ep->c_pid, ep->p_pid, ep->comm, ep->args);
        if (hashmap__find(env.comms, &ep->c_pid, NULL))
        {
            // pid already exists
            break;
        }
        else
        {
            // new pid
            // Save cpid -> comm
            char *comm_arr_c = malloc(TASK_COMM_LEN);
            char *comm_arr_p = malloc(TASK_COMM_LEN);
            strncpy(comm_arr_c, ep->comm, TASK_COMM_LEN);
            strncpy(comm_arr_p, ep->comm, TASK_COMM_LEN);
            hashmap__insert(env.comms, &ep->c_pid, comm_arr_c, HASHMAP_ADD, NULL, NULL);
            hashmap__insert(env.comms, &ep->c_pid, comm_arr_p, HASHMAP_ADD, NULL, NULL);

            // Save cpid -> ppid
            int *ppid = malloc(sizeof(int));
            *ppid = ep->p_pid;
            bpf_map_update_elem(env.relations_map_fd, &ep->c_pid, ppid, 0);
            // hashmap__insert(env.relations, &ep->c_pid, ppid, HASHMAP_ADD, NULL, NULL);

            // Save cpid -> args, ppid -> args
            extract_process_args(env.args, ep->args, MAX_ARR_LEN);
            int arg_len = strlen(env.args);
            char *p_args = malloc(arg_len + 1);
            memset(p_args, 0, arg_len + 1);
            char *c_args = malloc(arg_len + 1);
            memset(c_args, 0, arg_len + 1);
            strncpy(c_args, env.args, arg_len + 1);
            strncpy(c_args, env.args, arg_len + 1);
            p_args[arg_len] = '\0';
            c_args[arg_len] = '\0';
            hashmap__insert(env.argses, &ep->p_pid, p_args, HASHMAP_ADD, NULL, NULL);
            hashmap__insert(env.argses, &ep->c_pid, c_args, HASHMAP_ADD, NULL, NULL);
            memset(env.args, 0, MAX_ARR_LEN);
            break;
        }
        break;
    case 1:
        if (hashmap__find(env.argses, &ep->c_pid, &arg_ptr))
        {
            extract_process_args(env.args, ep->args, MAX_ARR_LEN);
            // pid exists, update args
            int len_old = strlen(arg_ptr);
            int len_new = strlen(env.args);
            int len_total = len_old + len_new;
            char *new_arg_ptr = malloc(len_total + 2);
            strncpy(new_arg_ptr, arg_ptr, len_old);
            new_arg_ptr[len_old] = ',';
            strncpy(new_arg_ptr + len_old + 1, env.args, len_new + 1);
            new_arg_ptr[len_old + len_new + 1] = '\0';
            free(arg_ptr);
            hashmap__update(env.argses, &ep->c_pid, new_arg_ptr, NULL, NULL);
            hashmap__update(env.comms, &ep->c_pid, &ep->comm, NULL, NULL);
            break;
        }
        else
        {
            // ignore
            break;
        }
        memset(env.args, 0, MAX_ARR_LEN);
        break;
    case 2:
        find = hashmap__find(env.comms, &ep->c_pid, NULL);
        break;
    }
}

/**
 * Handle lost events
 */
static void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt)
{
    printf("Lost %llu events on CPU #%d!\n", lost_cnt, cpu);
}

void event_printer(int perf_map_fd)
{
    struct perf_buffer_opts pb_opts = {
        .sample_cb = handle_event,
        .lost_cb = handle_lost_events,
    };
    struct perf_buffer *pb = NULL;
    int err;

    pb = perf_buffer__new(perf_map_fd, 128, &pb_opts);
    err = libbpf_get_error(pb);
    if (err)
    {
        pb = NULL;
        printf("failed to open perf buffer: %d\n", err);
        goto cleanup;
    }
    while (env.loop_flag)
    {
        err = perf_buffer__poll(pb, 100);
        if (err < 0 && errno != EINTR)
        {
            printf("Error polling perf buffer: %d\n", err);
            goto cleanup;
        }
    }
cleanup:
    perf_buffer__free(pb);
}

int main(int argc, char **argv)
{
    struct process_tree_bpf *obj;
    int err;

    env.comms = hashmap__new(&int_hash_fn, &int_hash_fn_equal, NULL);
    env.argses = hashmap__new(&int_hash_fn, &int_hash_fn_equal, NULL);
    memset(env.comm, 0, TASK_COMM_LEN);

    DECLARE_LIBBPF_OPTS(bpf_object_open_opts, open_opts);
    static const struct argp argp = {
        .options = process_tree_options,
        .parser = parse_arg,
        .doc = argp_program_doc,
        .args_doc = NULL,
    };

    libbpf_set_print(libbpf_print_fn);
    bump_memlock_rlimit();
    err = argp_parse(&argp, argc, argv, 0, NULL, NULL);
    if (err)
        return err;

    open_opts.btf_custom_path = env.btf_custom_path;
    obj = process_tree_bpf__open_opts(&open_opts);
    if (!obj)
    {
        printf("failed to open BPF object\n");
        return 1;
    }

    err = process_tree_bpf__load(obj);
    if (err)
    {
        printf("failed to load BPF object: %d\n", err);
        goto cleanup;
    }

    // Pass params to ebpf program
    int map_fd = bpf_map__fd(obj->maps.args_event);
    int i = 0;
    struct args args = {};
    args.pid = env.pid;
    err = bpf_map_update_elem(map_fd, &i, &args, 0);

    env.relations_map_fd = bpf_map__fd(obj->maps.pid_map_event);

    if (err)
    {
        printf("failed to update args map\n");
        goto cleanup;
    }

    err = process_tree_bpf__attach(obj);
    if (err)
    {
        printf("failed to attach BPF programs: %s\n", strerror(-err));
        goto cleanup;
    }

    signal(SIGALRM, signal_alarm_func);
    alarm(env.duration);
    event_printer(bpf_map__fd(obj->maps.e_process_chain));
cleanup:
    // destory the bpf program
    process_tree_bpf__destroy(obj);
    return 0;
}