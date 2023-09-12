#ifndef _RTDELAY_H
#define _RTDELAY_H

#define TASK_COMM_LEN		16

enum support_syscall_e {
  FuncUnknown,
  FuncWrite,
  FuncRead,
  FuncSend,
  FuncRecv,
  FuncSendTo,
  FuncRecvFrom,
  FuncSendMsg,
  FuncRecvMsg,
  FuncMmap,
  FuncSockAlloc,
  FuncAccept,
  FuncAccept4,
  FuncSecuritySendMsg,
  FuncSecurityRecvMsg,
};

struct conn_param_t {
    const struct sockaddr *addr;
    int32_t fd;
};


struct data_param_t {
    enum support_syscall_e syscall_func;
    bool real_conn;
    int32_t fd;
    const char *buf;
    const struct iovec *iov;
    size_t iovlen;
    unsigned int *msg_len;
};

struct bpfarg {
	// bool kernel_threads_only;
	// bool user_threads_only;
	// __u64 max_block_ns;
	// __u64 min_block_ns;
	pid_t targ_tgid;
    pid_t server_pid;
	// pid_t targ_pid;
	// long state;
};

struct config_info_t {
	int32_t port;
	int32_t self_pid;
	int32_t data_sample;
    int32_t threhold_ms;
};

struct perf_test {
    enum support_syscall_e syscall_func;
    int32_t fd;
    uint64_t start_ts;
    uint64_t id;
};

struct key_on {
	uint32_t pid;
};

struct val_t_on {
    uint64_t delta;
    int flag;
    uint64_t runqueue;
    uint64_t rtlatency;
    uint64_t server_delta;
};


struct val_t {
	uint64_t delta;
	// char comm[TASK_COMM_LEN];
};

struct perf_oncpu {
    uint64_t delta;
    uint64_t read_ts;
    uint64_t now_ts;
  // int flag;
};

struct key_t {
	// uint32_t pid;
	// uint32_t tgid;
	// int user_stack_id;
    uint64_t read_ts;
	int kern_stack_id;
};


struct read_key {
    uint64_t read_ts;
};


struct stacks_q{
    int kern_stack_id;
    uint64_t read_ts;
};


enum OFFCPU_REASON{
	FUTEX_R,
    LOCK_R,
	IO_R,
	NET_R,
    SERVER_R,
    OTHER_R,
    UNKNOWN_R,
};

#endif
