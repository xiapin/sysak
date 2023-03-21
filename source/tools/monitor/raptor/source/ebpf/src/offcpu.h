#ifndef __OFF_CPU_H
#define __OFF_CPU_H

struct profile_key_t {
    __u32 pid;
    __s64 user_stack;
    __s64 kernel_stack;
    char comm[16];
};

struct value_t {
    __u64 counts;
    __u64 deltas;
};

struct {
    __uint(type, BPF_MAP_TYPE_STACK_TRACE);
    __uint(key_size, sizeof(__u32));
    __uint(value_size, 100 * sizeof(__u64));
    __uint(max_entries, 10000);
} stacks SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_LRU_HASH);
	__uint(key_size, sizeof(__u32));
	__uint(value_size, sizeof(__u64));
    __uint(max_entries, 10000);
} starts SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, struct key_t);
	__type(value, struct value_t);
	__uint(max_entries, 10000);
} counts SEC(".maps");

struct task_struct {
	__u32 pid;
    __u32 tgid;
}  __attribute__((preserve_access_index));

#endif