#ifndef __KVMEXITTIME_H
#define __KVMEXITTIME_H

struct kvm_exit_timestamps {
    unsigned int exit_reason;
    __u64 kvm_exit_timestamp;
    __u64 sched_switch_timestamp;
    __u64 sched_time;
};

struct kvm_exit_time {
    __u64 cumulative_time;
    __u64 cumulative_sched_time;
    __u64 count;
};

struct args {
	pid_t targ_pid;
	pid_t targ_tgid;
};

#endif /* __KVMEXITTIME_H */