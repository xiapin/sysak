#ifndef __FILTER_H
#define __FILTER_H

struct filter
{
    int pid;
    unsigned long long threshold;
    unsigned int protocol;
    unsigned short be_lport;
    unsigned short be_rport;
    unsigned short lport;
    unsigned short rport;
    unsigned long long sock;
};

#ifdef __VMLINUX_H__

struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, u32);
    __type(value, struct filter);
} filters SEC(".maps");

static __always_inline struct filter *get_filter()
{
    int key = 0;
    return bpf_map_lookup_elem(&filters, &key);
}

#endif

#endif