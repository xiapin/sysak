
#ifndef __PERF_H
#define __PERF_H

#define MAX_QUEUE_NUM 32
#define	NETDEV_ALIGN		32


struct virtio_ring
{
    unsigned short len;
    unsigned short last_used_idx;
    unsigned short avail_idx;
    unsigned short used_idx;
};

struct virito_queue
{
    int pid;

    int sq_size;
    int rq_size;

    struct virtio_ring rxs[MAX_QUEUE_NUM];
    struct virtio_ring txs[MAX_QUEUE_NUM];
    
    unsigned int tx_idx;
    unsigned int rx_idx;
};


#endif
