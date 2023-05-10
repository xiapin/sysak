#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>
#include "../ioctl_cnt.h"

#define PERF_MAX_STACK_DEPTH	127
#define KERN_STACKID_FLAGS	(0 | BPF_F_FAST_STACK_CMP)
#define _(P) ({						\
	typeof(P) val;					\
	__builtin_memset(&val, 0, sizeof(val));		\
	bpf_probe_read(&val, sizeof(val), &P);		\
	val;						\
})

static inline int strequal(const char *src, const char *dst)
{
	bool ret = true;
	int i;
	unsigned char c1, c2;

	#pragma clang loop unroll(full)
	for (int i = 0; i < 16; i++) {
		c1 = *src++;
		c2 = *dst++;
		if (!c1 || !c2) {
			if (c1 != c2)
				ret = false;
			else
				ret = true;
			break;
		}
		if (c1 != c2) {
			ret = false;
			break;
		}
	}
	return ret;
}

struct bpf_map_def SEC("maps") stackmap = {
	.type = BPF_MAP_TYPE_STACK_TRACE,
	.key_size = sizeof(u32),
	.value_size = PERF_MAX_STACK_DEPTH * sizeof(u64),
	.max_entries = 10240,
};

struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
	__uint(max_entries, 2);
	__type(key, u32);
	__type(value, struct arg_info);
} cnt_map SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 128);
	__type(key, int);
	__type(value, int);
} sysnr_map SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
	__uint(key_size, sizeof(u32));
	__uint(value_size, sizeof(u32));
} events SEC(".maps");

struct ioctl_arg {
	int nr;
	unsigned int fd;
	unsigned int cmd;
	unsigned long arg;
};

SEC("kprobe/drm_ioctl")
int BPF_KPROBE(kp_drm_ioctl, int nr, unsigned int fd,  
		unsigned int cmd, unsigned long arg)
{
	int key;
	u64 *cntp;

	key = 0;
	pcnt = bpf_map_lookup_elem(&cnt_map, &key);
	if (pcnt) {
		cnt = *cntp;
		*cntp = cnt + 1;
	}
}
char LICENSE[] SEC("license") = "GPL";
