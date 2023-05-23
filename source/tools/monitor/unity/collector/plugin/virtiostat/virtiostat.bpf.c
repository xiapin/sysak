//
// Created by 廖肇燕 on 2023/5/7.
//

#define BPF_NO_GLOBAL_DATA

#include <vmlinux.h>
#include <coolbpf.h>
#include "virtiostat.h"

BPF_HASH(stats, u64, virtio_stat_t, 256);

struct virtio_device_id {
    u32 device;
    u32 vendor;
};

struct virtio_device {
    int index;
    bool failed;
    bool config_enabled;
    bool config_change_pending;
    spinlock_t config_lock;
    struct device dev;
    struct virtio_device_id id;
    void *config;
    void *vringh_config;
    struct list_head vqs;
    u64 features;
    void *priv;
};

struct virtqueue {
    struct list_head list;
    void (*callback)(struct virtqueue *vq);
    const char *name;
    struct virtio_device *vdev;
    unsigned int index;
    unsigned int num_free;
    void *priv;
};

static inline void add_value(virtio_stat_t *vs, struct scatterlist **sgs,
        u32 out_sgs, u32 in_sgs) {
    vs->out_sgs += out_sgs;
    vs->in_sgs += in_sgs;
}

static inline void record(struct virtqueue *vq, struct scatterlist **sgs,
                   unsigned int out_sgs, unsigned int in_sgs)
{
    virtio_stat_t newvs = {0};
    virtio_stat_t *vs;
    u64 key = (u64)vq;

    vs = bpf_map_lookup_elem(&stats, &key);
    if (!vs) {
        bpf_probe_read_kernel_str(newvs.driver, sizeof(newvs.driver), BPF_CORE_READ(vq, vdev, dev.driver, name));
        bpf_probe_read_kernel_str(newvs.dev, sizeof(newvs.dev), BPF_CORE_READ(vq, vdev, dev.kobj.name));
        bpf_probe_read_kernel_str(newvs.vqname, sizeof(newvs.vqname), BPF_CORE_READ(vq, name));

        add_value(&newvs, sgs, out_sgs, in_sgs);
        bpf_map_update_elem(&stats, &key, &newvs, BPF_ANY);
    } else {
        add_value(vs, sgs, out_sgs, in_sgs);
    }
}

SEC("kprobe/virtqueue_add_sgs")
int trace_virtqueue_add_sgs(struct pt_regs *ctx)
{
    struct virtqueue *vq = (struct virtqueue *)PT_REGS_PARM1(ctx);
    struct scatterlist **sgs = (struct scatterlist **)PT_REGS_PARM2(ctx);
    u32 out_sgs = PT_REGS_PARM3(ctx);
    u32 in_sgs  = PT_REGS_PARM4(ctx);
    record(vq, sgs, out_sgs, in_sgs);
    return 0;
}

SEC("kprobe/virtqueue_add_outbuf")
int trace_virtqueue_add_outbuf(struct pt_regs *ctx)
{
    struct virtqueue *vq = (struct virtqueue *)PT_REGS_PARM1(ctx);
    struct scatterlist **sgs = (struct scatterlist **)PT_REGS_PARM2(ctx);
    record(vq, sgs, 1, 0);
    return 0;
}

SEC("kprobe/virtqueue_add_inbuf")
int trace_virtqueue_add_inbuf(struct pt_regs *ctx)
{
    struct virtqueue *vq = (struct virtqueue *)PT_REGS_PARM1(ctx);
    struct scatterlist **sgs = (struct scatterlist **)PT_REGS_PARM2(ctx);
    record(vq, sgs, 0, 1);
    return 0;
}

SEC("kprobe/virtqueue_add_inbuf_ctx")
int trace_virtqueue_add_inbuf_ctx(struct pt_regs *ctx)
{
    struct virtqueue *vq = (struct virtqueue *)PT_REGS_PARM1(ctx);
    struct scatterlist **sgs = (struct scatterlist **)PT_REGS_PARM2(ctx);
    record(vq, sgs, 0, 1);
    return 0;
}
