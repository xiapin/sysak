#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_endian.h>

#include "virtio.h"

struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, u32);
    __type(value, struct virito_queue);
} imap SEC(".maps");

#define ALIGN(x, a) __ALIGN(x, (typeof(x))(a)-1)
#define __ALIGN(x, mask) (((x) + (mask)) & ~(mask))

struct virtnet_info {
	struct virtio_device *vdev;
	struct virtqueue *cvq;
	struct net_device *dev;
	struct send_queue *sq;
	struct receive_queue *rq;
};

struct send_queue {
	struct virtqueue *vq;
};

struct receive_queue {
	struct virtqueue *vq;
};

__always_inline struct virtnet_info *get_virtnet_info(struct net_device *dev)
{
    return (struct virtnet_info *)((char *)dev + ALIGN(bpf_core_type_size(struct net_device), NETDEV_ALIGN));
}


// for tx queue
SEC("kprobe/dev_id_show")
int BPF_KPROBE(kprobe_dev_id_show, struct device *device)
{
    struct net_device *dev = container_of(device, struct net_device, dev);
    struct virtnet_info *vi = get_virtnet_info(dev);
    int key = 0;

    struct virito_queue *qs = bpf_map_lookup_elem(&imap, &key);
    if (!qs)
        return 0;

    int pid = bpf_get_current_pid_tgid() >> 32;
    if (qs->pid != pid)
        return 0;

    int tx = qs->tx_idx;
    qs->tx_idx++;

    struct send_queue *sq;
    bpf_probe_read(&sq, sizeof(sq), &vi->sq);
    sq = (char *)sq + tx * qs->sq_size;
    struct virtqueue *vq;
    bpf_probe_read(&vq, sizeof(vq), &sq->vq);
    struct vring_virtqueue *vvq = container_of(vq, struct vring_virtqueue, vq);
    struct vring vring;
    bpf_probe_read(&vring, sizeof(vring), &vvq->split.vring);
    struct virtio_ring *ring = &qs->txs[tx & (MAX_QUEUE_NUM - 1)];

    bpf_probe_read(&ring->avail_idx, sizeof(u16), &vring.avail->idx);
    bpf_probe_read(&ring->used_idx, sizeof(u16), &vring.used->idx);
    bpf_probe_read(&ring->last_used_idx, sizeof(u16), &vvq->last_used_idx);
    ring->len = vring.num;

    return 0;
}

// for rx queue
SEC("kprobe/dev_port_show")
int BPF_KPROBE(kprobe_dev_port_show, struct device *device)
{
    struct net_device *dev = container_of(device, struct net_device, dev);
    struct virtnet_info *vi = get_virtnet_info(dev);
    int key = 0;

    struct virito_queue *qs = bpf_map_lookup_elem(&imap, &key);
    if (!qs)
        return 0;

    int pid = bpf_get_current_pid_tgid() >> 32;
    if (qs->pid != pid)
        return 0;

    u64 rx = qs->rx_idx;
    qs->rx_idx++;
    if (rx >= MAX_QUEUE_NUM)
        return 0;

    struct receive_queue *rq;
    bpf_probe_read(&rq, sizeof(rq), &vi->rq);
    rq = (char *)rq + rx * qs->rq_size;
    struct virtqueue *vq;
    bpf_probe_read(&vq, sizeof(vq), &rq->vq);
    struct vring_virtqueue *vvq = container_of(vq, struct vring_virtqueue, vq);
    struct vring vring;
    bpf_probe_read(&vring, sizeof(vring), &vvq->split.vring);

    struct virtio_ring *ring = &qs->rxs[rx & (MAX_QUEUE_NUM - 1)];

    bpf_probe_read(&ring->avail_idx, sizeof(u16), &vring.avail->idx);
    bpf_probe_read(&ring->used_idx, sizeof(u16), &vring.used->idx);
    bpf_probe_read(&ring->last_used_idx, sizeof(u16), &vvq->last_used_idx);
    ring->len = vring.num;
    return 0;
}

char _license[] SEC("license") = "GPL";