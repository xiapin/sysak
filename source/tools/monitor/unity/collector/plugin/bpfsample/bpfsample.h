

#ifndef BPF_SAMPLE_H
#define BPF_SAMPLE_H

#ifndef __VMLINUX_H__

#include "../plugin_head.h"

#define DEFINE_SEKL_OBJECT(skel_name) \
    struct skel_name##_bpf *skel_name = NULL;

#define LOAD_SKEL_OBJECT(skel_name)                                                                    \
    (                                                                                                  \
        {                                                                                              \
            __label__ load_bpf_skel_out;                                                               \
            int __ret = 0;                                                                             \
            skel_name = skel_name##_bpf__open();                                                       \
            if (!skel_name)                                                                            \
            {                                                                                          \
                printf("failed to open BPF object\n");                                                 \
                __ret = -1;                                                                            \
                goto load_bpf_skel_out;                                                                \
            }                                                                                          \
            __ret = skel_name##_bpf__load(skel_name);                                                  \
            if (__ret)                                                                                 \
            {                                                                                          \
                printf("failed to load BPF object: %d\n", __ret);                                      \
                DESTORY_SKEL_BOJECT(skel_name);                                                        \
                goto load_bpf_skel_out;                                                                \
            }                                                                                          \
            __ret = skel_name##_bpf__attach(skel_name);                                                \
            if (__ret)                                                                                 \
            {                                                                                          \
                printf("failed to attach BPF programs: %s\n", strerror(-__ret));                       \
                DESTORY_SKEL_BOJECT(skel_name);                                                        \
                goto load_bpf_skel_out;                                                                \
            }                                                                                          \
        load_bpf_skel_out:                                                                             \
            __ret;                                                                                     \
        })

#define DESTORY_SKEL_BOJECT(skel_name) \
    skel_name##_bpf__destroy(skel_name);

int init(void *arg);
int call(int t, struct unity_lines *lines);
void deinit(void);

#endif

#endif
