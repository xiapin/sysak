#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <sys/resource.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <string.h>
#include "bpf/rtdelay.skel.h"
#include "trace_helpers.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/sysinfo.h>

#include "rtdelay.h"
#include "rtdelay_api.h"

static volatile bool exiting = false;
#define TASK_COMM_LEN 16
#define MAX_FILENAME_LEN 512

#define debug_print 0

#define output_sysom 1

static struct env
{
    pid_t pid;
    bool verbose;
    int perf_max_stack_depth;
    int stack_storage_size;
    time_t duration;
} env = {
    .pid = -1,
    .verbose = false,
    .perf_max_stack_depth = 127,
    .stack_storage_size = 1024,
};

struct StackReason
{
    char *stack;
    enum OFFCPU_REASON reason;
};

struct FindList
{
    struct StackReason data;
    struct FindList *next;
};
static struct FindList FL;

struct Request
{
    __u64 read_ts;
    __u64 oncpu_t;
    __u64 runqueue_t;
    __u64 rtlatency_t;
    __u64 offcpu_FUTEX_t;
    __u64 offcpu_IO_t;
    __u64 offcpu_NET_t;
    __u64 offcpu_OTHER_t;
    __u64 offcpu_SERVER_t;
    __u64 offcpu_LOCK_t;
    struct Request *next;
};

time_t get_boot_time()
{
    struct sysinfo info;
    time_t cur_time = 0;
    time_t boot_time = 0;
    if (sysinfo(&info))
    {
        printf("Failed to get sysinfo,  reason");
        return 0;
    }
    time(&cur_time);
    if (cur_time > info.uptime)
    {
        boot_time = cur_time - info.uptime;
    }
    return boot_time;
}

void initData(struct FindList *head)
{
    char *stacks[14] = {"futex_wait", "net_write", "net_read", "mutex_lock", "sys_recvfrom", "ep_poll", "do_sys_poll", "do_sys_openat", "tcp_recvmsg", "vfs_", "ext4_", "block_", "blk_"};
    enum OFFCPU_REASON reasons[14] = {FUTEX_R, NET_R, NET_R, LOCK_R, NET_R, FUTEX_R, FUTEX_R, IO_R, NET_R, IO_R, IO_R, IO_R, IO_R};
    struct FindList *p = head;
    int i;
    for (i = 0; i < sizeof(stacks); i++)
    {
        if (!stacks[i])
        {
            break;
        }
        struct FindList *temp = (struct FindList *)malloc(sizeof(struct FindList));
        (temp->data).stack = stacks[i];
        // strcpy((temp->data).stack,stacks[i]);
        (temp->data).reason = reasons[i];
        p->next = temp;
        p = p->next;
    }
    p->next = NULL;
}

enum OFFCPU_REASON search_by_stack(struct FindList *head, const char *stack)
{
    enum OFFCPU_REASON o_r = UNKNOWN_R;
    struct FindList *p = head;
    p = p->next;
    while (p != NULL)
    {
        if (sizeof((p->data).stack) <= 1)
        {
            p = p->next;
            continue;
        }
        if (strcasestr(stack, (p->data).stack))
        {
            o_r = (p->data).reason;
            break;
        }
        else
        {
            p = p->next;
        }
    }
    return o_r;
}

static void sig_handler(int sig)
{
    exiting = true;
}

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
    if (level == LIBBPF_DEBUG && !env.verbose)
        return 0;
    return vfprintf(stderr, format, args);
}

static void bump_memlock_rlimit(void)
{
    struct rlimit rlim_new = {
        .rlim_cur = RLIM_INFINITY,
        .rlim_max = RLIM_INFINITY,
    };

    if (setrlimit(RLIMIT_MEMLOCK, &rlim_new))
    {
        fprintf(stderr, "Failed to increase RLIMIT_MEMLOCK limit!\n");
        exit(1);
    }
}

enum OFFCPU_REASON find_offcpu_reason(int stack_id, struct rtdelay_bpf *obj)
{
    int rmap, err;
    enum OFFCPU_REASON off_reason;
    rmap = bpf_map__fd(obj->maps.reason_map);
    err = bpf_map_lookup_elem(rmap, &stack_id, &off_reason);
    if (err < 0 || err == ENOENT)
    {
        return UNKNOWN_R;
    }
    return off_reason;
}

enum OFFCPU_REASON add_offcpu_reason(struct ksyms *ksyms, struct rtdelay_bpf *obj, int stack_id)
{
    __u64 *ip_k;
    const struct ksym *ksym;
    int sfd, rmp;
    sfd = bpf_map__fd(obj->maps.stackmap);
    rmp = bpf_map__fd(obj->maps.reason_map);
    enum OFFCPU_REASON offcpu_reason;

    ip_k = calloc(env.perf_max_stack_depth, sizeof(*ip_k));
    if (!ip_k)
    {
        fprintf(stderr, "failed to alloc ip_k\n");
        goto cleanup;
    }

    if (bpf_map_lookup_elem(sfd, &stack_id, ip_k) != 0)
    {
#if debug_print
        printf("[Missed Kernel Stack];");
#endif
        goto cleanup;
    }
    int i = 0;

    for (i = 0; i < env.perf_max_stack_depth && ip_k[i]; i++)
    {
        ksym = ksyms__map_addr(ksyms, ip_k[i]);

        if (!ksym)
        {
            continue;
        }
        // print stacks
#if debug_print
        printf("%s;", ksym ? ksym->name : "Unknown");
#endif
        offcpu_reason = search_by_stack(&FL, ksym->name);
        if (offcpu_reason != UNKNOWN_R)
        {
            bpf_map_update_elem(rmp, &stack_id, &offcpu_reason, BPF_NOEXIST);
            return offcpu_reason;
        }
    }
cleanup:
    free(ip_k);
    offcpu_reason = OTHER_R;
    bpf_map_update_elem(rmp, &stack_id, &offcpu_reason, BPF_NOEXIST);
    return OTHER_R;
}

static void add_offcpu_delta(struct Request *request, enum OFFCPU_REASON reason, int delta)
{
    switch (reason)
    {
    case FUTEX_R:
        __sync_fetch_and_add(&request->offcpu_FUTEX_t, delta);
        break;
    case IO_R:
        __sync_fetch_and_add(&request->offcpu_IO_t, delta);
        break;
    case NET_R:
        __sync_fetch_and_add(&request->offcpu_NET_t, delta);
        break;
    case LOCK_R:
        __sync_fetch_and_add(&request->offcpu_LOCK_t, delta);
        break;
    case SERVER_R:
        __sync_fetch_and_add(&request->offcpu_SERVER_t, delta);
        break;
    case UNKNOWN_R:
    case OTHER_R:
        __sync_fetch_and_add(&request->offcpu_OTHER_t, delta);
        break;
    }
}

static void offcpu_analyse(struct ksyms *ksyms,
                           struct rtdelay_bpf *obj, const struct read_key *key, struct Request *request)
{
    int err, ifd_on, rfd, rsfd, ifd;
    struct val_t_on val_on;
    struct stacks_q head, stack_next;
    struct key_t k_t = {};
    struct val_t val_off;
    time_t boot_time = get_boot_time();

    ifd = bpf_map__fd(obj->maps.info_off);
    ifd_on = bpf_map__fd(obj->maps.info_on);
    rfd = bpf_map__fd(obj->maps.request_head);
    rsfd = bpf_map__fd(obj->maps.request_stacks);

    // oncpu time
    err = bpf_map_lookup_elem(ifd_on, key, &val_on);
    if (err < 0)
    {
        fprintf(stderr, "failed to lookup info: %d\n", err);
        return;
    }
#if debug_print
    printf("%ld;%ld\n", val_on.delta, key->read_ts);
#endif
    request->read_ts = key->read_ts / 1e9 + boot_time;
    request->oncpu_t = val_on.delta;
    request->runqueue_t = val_on.runqueue;
    request->rtlatency_t = val_on.rtlatency;
    request->offcpu_SERVER_t = val_on.server_delta;
    k_t.read_ts = key->read_ts;

#if 0
    printf("buf = %s\n", val_on.buf);
#endif

#if debug_print
    printf("k_t.read_ts:%ld\n", k_t.read_ts);
#endif
    // find reason for offcpu
    err = bpf_map_lookup_elem(rfd, &key->read_ts, &head);
    if (err < 0)
    {
        fprintf(stderr, "failed to lookup info: %d\n", err);
        return;
    }
    while (head.kern_stack_id != 0)
    {
#if debug_print
        printf("stack_id:=%d\n", head.kern_stack_id);
#endif
        k_t.kern_stack_id = head.kern_stack_id;
        // find existing reason
        enum OFFCPU_REASON off_reason;
        off_reason = find_offcpu_reason(head.kern_stack_id, obj);
        if (off_reason == UNKNOWN_R)
        {
            // add reason
            off_reason = add_offcpu_reason(ksyms, obj, head.kern_stack_id);
        }

        // add delta
        err = bpf_map_lookup_elem(ifd, &k_t, &val_off);
        if (err < 0)
        {
            fprintf(stderr, "failed to lookup info: %d\n", err);
            return;
        }
        add_offcpu_delta(request, off_reason, val_off.delta);
#if debug_print
        printf("=====>offcpu_reason=%d,%d\n", off_reason, val_off.delta);
#endif
        // next stacks
        err = bpf_map_lookup_elem(rsfd, &head, &stack_next);
        if (err == ENOENT)
        {
            return;
        }
        head.kern_stack_id = stack_next.kern_stack_id;
    }
    if (request->offcpu_NET_t > request->offcpu_SERVER_t)
    {
        request->offcpu_NET_t -= request->offcpu_SERVER_t;
    }
    else
    {
        request->offcpu_SERVER_t = request->offcpu_NET_t;
        request->offcpu_NET_t = 0;
    }
}

static void analyse(struct ksyms *ksyms,
                    struct rtdelay_bpf *obj)
{
    struct read_key lookup_key = {}, next_key;
    int err, ifd_on;

    ifd_on = bpf_map__fd(obj->maps.info_on);
    printf("[");

    while (!bpf_map_get_next_key(ifd_on, &lookup_key, &next_key))
    {
        struct Request r = {};
        struct val_t_on val_on;
        err = bpf_map_lookup_elem(ifd_on, &next_key, &val_on);
        if (val_on.flag == 0)
        {
            lookup_key = next_key;
            continue;
        }
        offcpu_analyse(ksyms, obj, &next_key, &r);
        lookup_key = next_key;

        if (!output_sysom)
            printf("read_ts:%lld, on:%lld, runqueue:%lld, rt_latency:%lld, futex:%lld, lock:%lld, io:%lld, net:%lld, server:%lld, other:%lld\n", r.read_ts, r.oncpu_t, r.runqueue_t, r.rtlatency_t, r.offcpu_FUTEX_t, r.offcpu_LOCK_t, r.offcpu_IO_t, r.offcpu_NET_t, r.offcpu_SERVER_t, r.offcpu_OTHER_t);
        else
            printf("{\"read_ts\":%lld, \"on\":%lld, \"runqueue\":%lld, \"rt_latency\":%lld, \"futex\":%lld, \"lock\":%lld, \"io\":%lld, \"net\":%lld, \"server\":%lld, \"other\":%lld},\n", r.read_ts, r.oncpu_t, r.runqueue_t, r.rtlatency_t, r.offcpu_FUTEX_t, r.offcpu_LOCK_t, r.offcpu_IO_t, r.offcpu_NET_t, r.offcpu_SERVER_t, r.offcpu_OTHER_t);
    }

    printf("{}]");
}

int rtdelay(pid_t pid, pid_t server_pid, int duration)
{
    initData(&FL);

    struct rtdelay_bpf *skel;
    int err, argfd, key = 0;
    struct ksyms *ksyms = NULL;
    // struct syms_cache *syms_cache = NULL;
    struct bpfarg bpfarg;

    /* Set up libbpf errors and debug info callback */
    libbpf_set_print(libbpf_print_fn);
    bump_memlock_rlimit();
    /* Cleaner handling of Ctrl-C */
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    /* Load and verify BPF application */
    skel = rtdelay_bpf__open();
    if (!skel)
    {
        fprintf(stderr, "Failed to open and load BPF skeleton\n");
        return 1;
    }

    /* Load & verify BPF programs */
    err = rtdelay_bpf__load(skel);
    if (err)
    {
        fprintf(stderr, "Failed to load and verify BPF skeleton\n");
        goto cleanup;
    }

    argfd = bpf_map__fd(skel->maps.argmap);
    bpfarg.targ_tgid = pid;
    bpfarg.server_pid = server_pid;
    bpf_map_update_elem(argfd, &key, &bpfarg, 0);

    ksyms = ksyms__load();
    if (!ksyms)
    {
        fprintf(stderr, "failed to load kallsyms\n");
        goto cleanup;
    }

    /* Attach tracepoints */
    err = rtdelay_bpf__attach(skel);
    if (err)
    {
        fprintf(stderr, "Failed to attach BPF skeleton\n");
        goto cleanup;
    }

    sleep(duration);
    analyse(ksyms, skel);

cleanup:
    /* Clean up */

    rtdelay_bpf__destroy(skel);
    ksyms__free(ksyms);

    return err < 0 ? -err : 0;
}
