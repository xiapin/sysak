#include "bpf_iosdiag_common.h"

struct scsi_cmnd___self {
	struct request *request;
};

SEC("kprobe/scsi_dispatch_cmd")
int kprobe_scsi_dispatch_cmd(struct pt_regs *ctx)
{
	struct scsi_cmnd___self *cmd = (struct scsi_cmnd *)PT_REGS_PARM1(ctx);
	struct request *req;

	bpf_core_read(&req, sizeof(struct request *), &cmd->request);
	if (!req) {
		//bpf_printk("kprobe_scsi_dispatch_cmd: con't get request");
		return 0;
	}
	return trace_io_driver_route(ctx, req, IO_ISSUE_DEVICE_POINT);
}

SEC("kprobe/scsi_done")
int kprobe_scsi_done(struct pt_regs *ctx)
{
	struct scsi_cmnd___self *cmd = (struct scsi_cmnd *)PT_REGS_PARM1(ctx);
	struct request *req;

	bpf_core_read(&req, sizeof(struct request *), &cmd->request);
	if (!req) {
		//bpf_printk("kprobe_scsi_done: con't get request");
		return 0;
	}
	return trace_io_driver_route(ctx, req, IO_RESPONCE_DRIVER_POINT);
}
