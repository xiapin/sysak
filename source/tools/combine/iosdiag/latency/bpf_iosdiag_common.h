#ifndef __BPF_IOSDIAG_COMMON_H
#define __BPF_IOSDIAG_COMMON_H

#include <linux/version.h>
#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_tracing.h>
#include "iosdiag.h"

struct bpf_map_def SEC("maps") iosdiag_maps = {
	.type = BPF_MAP_TYPE_HASH,
	.key_size = sizeof(struct iosdiag_key),
	.value_size = sizeof(struct iosdiag_req),
	.max_entries = 2048,
};

struct bpf_map_def SEC("maps") iosdiag_maps_targetdevt = {
	.type = BPF_MAP_TYPE_HASH,
	.key_size = sizeof(u32),
	.value_size = sizeof(u32),
	.max_entries = 1,
};

struct bpf_map_def SEC("maps") iosdiag_maps_notify = {
	.type = BPF_MAP_TYPE_PERF_EVENT_ARRAY,
	.key_size = sizeof(int),
	.value_size = sizeof(int),
};

inline int iosdiag_pkg_check(void *data, unsigned int len)
{
	return 1;
}

inline int get_target_devt(void)
{
	unsigned int key = 0;
	unsigned int *devt;

	devt = (unsigned int *)bpf_map_lookup_elem(&iosdiag_maps_targetdevt,
						   &key);
	if (devt)
		return *devt;
	return 0;
}

struct request___below_516 {
	struct gendisk *rq_disk;
};

struct request_queue___above_516 {
	struct gendisk *disk;
};

inline struct gendisk *get_rq_disk(struct request *req)
{
	struct request___below_516 *rq = req;
	struct request_queue___above_516 *q;
	struct gendisk *rq_disk;

	if (bpf_core_field_exists(rq->rq_disk)) {
		bpf_core_read(&rq_disk, sizeof(struct gendisk *), &rq->rq_disk);
	} else {
		bpf_probe_read(&q, sizeof(struct request_queue *), &req->q);
		bpf_core_read(&rq_disk, sizeof(struct gendisk *), &q->disk);
	}
	return rq_disk;
}

__always_inline void
init_iosdiag_key(unsigned long sector, unsigned int dev, struct iosdiag_key *key)
{
	key->sector = sector;
	key->dev = dev;
}

__always_inline int
trace_io_driver_route(struct pt_regs *ctx, struct request *req, enum ioroute_type type)
{
	struct iosdiag_req *ioreq;
	struct iosdiag_req new_ioreq = {0};
	struct iosdiag_req data = {0};
	struct iosdiag_key key = {0};
	unsigned long long now = bpf_ktime_get_ns();
	struct gendisk *rq_disk;
	int complete = 0;

	sector_t sector;
	dev_t devt = 0;
	int major = 0;
	int first_minor = 0;

	struct gendisk *gd = get_rq_disk(req);
	bpf_probe_read(&major, sizeof(int), &gd->major);
	bpf_probe_read(&first_minor, sizeof(int), &gd->first_minor);
	devt = ((major) << 20) | (first_minor);
	bpf_probe_read(&sector, sizeof(sector_t), &req->__sector);

	init_iosdiag_key(sector, devt, &key);

	ioreq = (struct iosdiag_req *)bpf_map_lookup_elem(&iosdiag_maps, &key);
	if (ioreq) {
		if (!ioreq->ts[type])
			ioreq->ts[type] = now;
		if (ioreq->diskname[0] == '\0') {
			rq_disk = get_rq_disk(req);
			bpf_probe_read(ioreq->diskname, sizeof(ioreq->diskname), &rq_disk->disk_name);
		}
		if (type == IO_RESPONCE_DRIVER_POINT) {
			ioreq->cpu[2] = bpf_get_smp_processor_id();
		}
		if (type == IO_DONE_POINT){
			if (ioreq->ts[IO_ISSUE_DEVICE_POINT] &&
		    	ioreq->ts[IO_RESPONCE_DRIVER_POINT])
				complete = 1;
		}
		if (complete) {
			memcpy(&data, ioreq, sizeof(data));
			bpf_perf_event_output(ctx, &iosdiag_maps_notify, 0xffffffffULL, &data, sizeof(data));
		}
	} else
		return 0;
	bpf_map_update_elem(&iosdiag_maps, &key, ioreq, BPF_ANY);
	return 0;
}

struct block_getrq_args {
	struct trace_entry ent;
	unsigned int dev;
	unsigned long sector;
	unsigned int nr_sector;
	char rwbs[8];
	char comm[16];
};

SEC("tracepoint/block/block_getrq")
int tracepoint_block_getrq(struct block_getrq_args *args)
{
	struct iosdiag_req new_ioreq = {0};
	struct iosdiag_key key = {0};
	unsigned long long now = bpf_ktime_get_ns();
	pid_t pid = bpf_get_current_pid_tgid();
	u32 target_devt = get_target_devt();

	if (target_devt && args->dev != target_devt)
		return 0;

	new_ioreq.cpu[0] = -1;
	new_ioreq.cpu[1] = -1;
	new_ioreq.cpu[2] = -1;
	new_ioreq.cpu[3] = -1;

	//bpf_printk("block_getrq: %d\n", args->dev);
	init_iosdiag_key(args->sector, args->dev, &key);
	if (pid)
		memcpy(new_ioreq.comm, args->comm, sizeof(args->comm));
	// IO_START_POINT
	new_ioreq.ts[IO_START_POINT] = now;
	new_ioreq.pid = pid;
	memcpy(new_ioreq.op, args->rwbs, sizeof(args->rwbs));
	new_ioreq.sector = args->sector;
	new_ioreq.data_len = args->nr_sector * 512;
	new_ioreq.cpu[0] = bpf_get_smp_processor_id();
	bpf_map_update_elem(&iosdiag_maps, &key, &new_ioreq, BPF_ANY);
	return 0;
}

struct block_rq_issue_args {
	struct trace_entry ent;
	unsigned int dev;
	unsigned long sector;
	unsigned int nr_sector;
	unsigned int bytes;
	char rwbs[8];
	char comm[16];
	char cmd[0];
};

SEC("tracepoint/block/block_rq_issue")
int tracepoint_block_rq_issue(struct block_rq_issue_args *args)
{
	struct iosdiag_req *ioreq;
	struct iosdiag_key key = {0};
	unsigned long long now = bpf_ktime_get_ns();
	pid_t pid = bpf_get_current_pid_tgid();
	int type = IO_ISSUE_DRIVER_POINT;
	u32 target_devt = get_target_devt();

	if (target_devt && args->dev != target_devt)
		return 0;

	init_iosdiag_key(args->sector, args->dev, &key);
	ioreq = (struct iosdiag_req *)bpf_map_lookup_elem(&iosdiag_maps, &key);
	if (ioreq) {
		if (ioreq->ts[type])
			type = IO_ISSUE_DEVICE_POINT;
		ioreq->ts[type] = now;

		if (args->bytes)
			ioreq->data_len = args->bytes;
		else if (args->nr_sector)
			ioreq->data_len = args->nr_sector * 512;
		ioreq->cpu[1] = bpf_get_smp_processor_id();
	} else
		return 0;
	bpf_map_update_elem(&iosdiag_maps, &key, ioreq, BPF_ANY);
	return 0;
}

struct block_rq_complete_args {
	struct trace_entry ent;
	dev_t dev;
	sector_t sector;
	unsigned int nr_sector;
	int errors;
	char rwbs[8];
	char cmd[0];
};

SEC("tracepoint/block/block_rq_complete")
int tracepoint_block_rq_complete(struct block_rq_complete_args *args)
{
	struct iosdiag_req *ioreq;
	struct iosdiag_req data = {0};
	struct iosdiag_key key = {0};
	unsigned long long now = bpf_ktime_get_ns();
	u32 target_devt = get_target_devt();
	int complete = 0;

	if (target_devt && args->dev != target_devt)
		//bpf_printk("block_rq_complete: %d, %d\n", args->dev, target_devt);
		return 0;
	
	init_iosdiag_key(args->sector, args->dev, &key);
	ioreq = (struct iosdiag_req *)bpf_map_lookup_elem(&iosdiag_maps, &key);
	if (ioreq) {
		if (!ioreq->ts[IO_COMPLETE_TIME_POINT])
			ioreq->ts[IO_COMPLETE_TIME_POINT] = now;
		ioreq->cpu[3] = bpf_get_smp_processor_id();
	} else
		return 0;
		
	bpf_map_update_elem(&iosdiag_maps, &key, ioreq, BPF_ANY);
	return 0;
}

SEC("kprobe/blk_account_io_done")
int kprobe_blk_account_io_done(struct pt_regs *ctx)
{
	struct request *req = (struct request *)PT_REGS_PARM1(ctx);
	struct iosdiag_key key = {0};

	sector_t sector;
	dev_t devt = 0;

	int major = 0;
	int first_minor = 0;

	struct gendisk *gd = get_rq_disk(req);
	bpf_probe_read(&major, sizeof(int), &gd->major);
	bpf_probe_read(&first_minor, sizeof(int), &gd->first_minor);
	devt = ((major) << 20) | (first_minor);
	bpf_probe_read(&sector, sizeof(sector_t), &req->__sector);

	init_iosdiag_key(sector, devt, &key);

	if (!req) {
		//bpf_printk("kprobe_blk_account_io_done: con't get request");
		return 0;
	}

	trace_io_driver_route(ctx, req, IO_DONE_POINT);
	bpf_map_delete_elem(&iosdiag_maps, &key);
	return 0;
}

#endif

