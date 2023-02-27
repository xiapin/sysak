//
// Created by 廖肇燕 on 2023/2/7.
//

#ifndef UNITY_BPF_HEAD_H
#define UNITY_BPF_HEAD_H
#include <coolbpf.h>

#ifdef COOLBPF_PERF_THREAD

#define DEFINE_SEKL_OBJECT(skel_name)                            \
    struct skel_name##_bpf *skel_name = NULL;                    \
    static pthread_t perf_thread = 0;                            \
    int thread_worker(struct beeQ *q, void *arg)                 \
    {                                                            \
        perf_thread_worker(arg);                                 \
        return 0;                                                \
    }                                                            \
    void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt)  \
    {                                                            \
        printf("Lost %llu events on CPU #%d!\n", lost_cnt, cpu); \
    }

#define LOAD_SKEL_OBJECT(skel_name, perf)                                                           \
    (                                                                                               \
        {                                                                                           \
            __label__ load_bpf_skel_out;                                                            \
            int __ret = 0;                                                                          \
            skel_name = skel_name##_bpf__open();                                                    \
            if (!skel_name)                                                                         \
            {                                                                                       \
                printf("failed to open BPF object\n");                                              \
                __ret = -1;                                                                         \
                goto load_bpf_skel_out;                                                             \
            }                                                                                       \
            __ret = skel_name##_bpf__load(skel_name);                                               \
            if (__ret)                                                                              \
            {                                                                                       \
                printf("failed to load BPF object: %d\n", __ret);                                   \
                DESTORY_SKEL_BOJECT(skel_name);                                                     \
                goto load_bpf_skel_out;                                                             \
            }                                                                                       \
            __ret = skel_name##_bpf__attach(skel_name);                                             \
            if (__ret)                                                                              \
            {                                                                                       \
                printf("failed to attach BPF programs: %s\n", strerror(-__ret));                    \
                DESTORY_SKEL_BOJECT(skel_name);                                                     \
                goto load_bpf_skel_out;                                                             \
            }                                                                                       \
            struct perf_thread_arguments *perf_args = calloc(1, sizeof(struct perf_thread_arguments)); \
            if (!perf_args)                                                                         \
            {                                                                                       \
                __ret = -ENOMEM;                                                                    \
                printf("failed to allocate memory: %s\n", strerror(-__ret));                        \
                DESTORY_SKEL_BOJECT(skel_name);                                                     \
                goto load_bpf_skel_out;                                                             \
            }                                                                                       \
            perf_args->mapfd = bpf_map__fd(skel_name->maps.perf);                                   \
            perf_args->sample_cb = handle_event;                                                    \
            perf_args->lost_cb = handle_lost_events;                                                \
            perf_args->ctx = arg;                                                                   \
            perf_thread = beeQ_send_thread(arg, perf_args, thread_worker);                          \
        load_bpf_skel_out:                                                                          \
            __ret;                                                                                  \
        })

#define DESTORY_SKEL_BOJECT(skel_name) \
    if (perf_thread > 0)               \
        kill_perf_thread(perf_thread); \
    skel_name##_bpf__destroy(skel_name);
#else
#define DEFINE_SEKL_OBJECT(skel_name)                            \
    struct skel_name##_bpf *skel_name = NULL;

#define LOAD_SKEL_OBJECT(skel_name, perf)                                                           \
    (                                                                                               \
        {                                                                                           \
            __label__ load_bpf_skel_out;                                                            \
            int __ret = 0;                                                                          \
            skel_name = skel_name##_bpf__open();                                                    \
            if (!skel_name)                                                                         \
            {                                                                                       \
                printf("failed to open BPF object\n");                                              \
                __ret = -1;                                                                         \
                goto load_bpf_skel_out;                                                             \
            }                                                                                       \
            __ret = skel_name##_bpf__load(skel_name);                                               \
            if (__ret)                                                                              \
            {                                                                                       \
                printf("failed to load BPF object: %d\n", __ret);                                   \
                DESTORY_SKEL_BOJECT(skel_name);                                                     \
                goto load_bpf_skel_out;                                                             \
            }                                                                                       \
            __ret = skel_name##_bpf__attach(skel_name);                                             \
            if (__ret)                                                                              \
            {                                                                                       \
                printf("failed to attach BPF programs: %s\n", strerror(-__ret));                    \
                DESTORY_SKEL_BOJECT(skel_name);                                                     \
                goto load_bpf_skel_out;                                                             \
            }                                                                                       \
        load_bpf_skel_out:                                                                          \
            __ret;                                                                                  \
        })

#define DESTORY_SKEL_BOJECT(skel_name) \
    skel_name##_bpf__destroy(skel_name);
#endif

#define coobpf_map_find(OBJ, NAME) bpf_object__find_map_fd_by_name(OBJ, NAME)
#define coobpf_key_next(FD, KEY, NEXT) bpf_map_get_next_key(FD, KEY, NEXT)
#define coobpf_key_value(FD, KEY, VALUE) bpf_map_lookup_elem(FD, KEY, VALUE)

#include "plugin_head.h"
#endif //UNITY_BPF_HEAD_H
