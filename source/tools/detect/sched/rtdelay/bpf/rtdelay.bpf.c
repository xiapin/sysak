#include <vmlinux.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include "../rtdelay.h"


#define PERF_MAX_STACK_DEPTH 127
#define MAX_ENTRIES		10240
#define KERN_STACKID_FLAGS      (0 | BPF_F_FAST_STACK_CMP)
#define MAX_PARAM_ENTRIES   8192

char LICENSE[] SEC("license") = "Dual BSD/GPL";

const   uint64_t NSEC_PER_SEC = 1000000000L;
const   uint64_t USER_HZ = 100;
const   int ConnStatsBytesThreshold = 131072;
const   int ConnStatsPacketsThreshold = 128;

struct psockaddr {
    struct sockaddr *addr;
    int sockfd;
};

struct send_server_param {
    struct sockaddr *addr;   //dest
    int sockfd;     //source
};

struct union_addr{
    // struct sockaddr saddr;
    struct sockaddr daddr;
};

struct {
	__uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
	__uint(key_size, sizeof(int));
	__uint(value_size, sizeof(int));
} test_map SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
	__uint(key_size, sizeof(int));
	__uint(value_size, sizeof(int));
} fd_map SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
	__uint(key_size, sizeof(int));
	__uint(value_size, sizeof(int));
} oncpu_poll_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, u64);
    __type(value, struct data_param_t);
    __uint(max_entries, MAX_PARAM_ENTRIES);
    __uint(map_flags, BPF_F_NO_PREALLOC);
} write_param_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, u16);
    __type(value, u64);
    __uint(max_entries, MAX_PARAM_ENTRIES);
    __uint(map_flags, BPF_F_NO_PREALLOC);
} port_readts_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, int);
    __type(value, u64);
    __uint(max_entries, MAX_PARAM_ENTRIES);
    __uint(map_flags, BPF_F_NO_PREALLOC);
} server_fd_time SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, u64);
    __type(value, struct data_param_t);
    __uint(max_entries, MAX_PARAM_ENTRIES);
    __uint(map_flags, BPF_F_NO_PREALLOC);
} read_param_map SEC(".maps");


struct internal_key_on {
	u64 start_ts;
  struct key_on key;
};

struct internal_key_off {
	u64 start_ts;
	struct key_t key;
};

struct internal_key_server {
    u64 read_ts;
    u64 start_ts;
};

struct r_fd {
  int fd;
};

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, u32);
	__type(value, struct r_fd);
	__uint(max_entries, MAX_ENTRIES);
} request_fd SEC(".maps");


struct bpf_map_def SEC("maps") stackmap = {
        .type = BPF_MAP_TYPE_STACK_TRACE,
        .key_size = sizeof(u32),
        .value_size = PERF_MAX_STACK_DEPTH * sizeof(u64),
        .max_entries = 10000,
};

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, u32);
	__type(value, enum OFFCPU_REASON);
	__uint(max_entries, MAX_ENTRIES);
} reason_map SEC(".maps");


struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 1);
	__type(key, u32);
	__type(value, struct bpfarg);
} argmap SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, struct read_key);
	__type(value, struct val_t_on);
	__uint(max_entries, MAX_ENTRIES);
} info_on SEC(".maps");


struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, struct key_t);
	__type(value, struct val_t);
	__uint(max_entries, MAX_ENTRIES);
} info_off SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, u64);
	__type(value, struct stacks_q);
	__uint(max_entries, MAX_ENTRIES);
} request_head SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, struct stacks_q);
	__type(value, struct stacks_q);
	__uint(max_entries, MAX_ENTRIES);
} request_stacks SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, u32);
	__type(value, u64);
	__uint(max_entries, MAX_ENTRIES);
} start_on SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, u32);
	__type(value, int);
	__uint(max_entries, MAX_ENTRIES);
} send_sockfd SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, u32);
	__type(value, struct internal_key_off);
	__uint(max_entries, MAX_ENTRIES);
} start_off SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, u32);
	__type(value, u64);
	__uint(max_entries, MAX_ENTRIES);
} start_runqueue SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, u32);
	__type(value, u64);
	__uint(max_entries, MAX_ENTRIES);
} read_time_map SEC(".maps");


#define _(P) ({typeof(P) val = 0; bpf_probe_read(&val, sizeof(val), &P); val;})
#define GETARG_FROM_ARRYMAP(map,argp,type,member,oldv)({	\
	type retval = (type)oldv;			\
	int i = 0;				\
	argp = bpf_map_lookup_elem(&map, &i);	\
	if (argp) {				\
		retval = _(argp->member);		\
	}					\
	retval;					\
	})

static bool allow_record(uint64_t id)
{
    pid_t targ_tgid;
    uint32_t tgid = id >> 32;
    struct bpfarg *argp;
    targ_tgid = GETARG_FROM_ARRYMAP(argmap, argp, pid_t, targ_tgid,-1);
    if (targ_tgid != -1 && targ_tgid != tgid)
        return false;
    return true;
}

static bool is_server_pid(uint64_t id)
{
    pid_t server_pid;
    uint32_t tgid = id >> 32;
    struct bpfarg *argp;
    server_pid = GETARG_FROM_ARRYMAP(argmap, argp, pid_t, server_pid,-1);
    if (server_pid != -1 && server_pid == tgid)
        return true;
    return false;
}

static bool allow_record_tgid(uint32_t tgid)
{
  /*filter with tgid*/
    pid_t targ_tgid;
    struct bpfarg *argp;
    targ_tgid = GETARG_FROM_ARRYMAP(argmap, argp, pid_t, targ_tgid,-1);
    if (targ_tgid != -1 && targ_tgid != tgid)
        return false;
    return true;
}

static void delete_unwrite_pid(uint32_t pid){
  /*delete information without write process*/
    u64 *read_ts;
    read_ts = bpf_map_lookup_elem(&read_time_map, &pid);
    if (!read_ts){
        return;
    }
    struct read_key r_k={};
    struct val_t_on *valp;
    r_k.read_ts = *read_ts;
    valp = bpf_map_lookup_elem(&info_on, &r_k);
    if (!valp)
    {
        return;
    }
    if (valp->flag==0){
        bpf_map_delete_elem(&info_on, &r_k);
    }
}

static u64 get_readts(uint32_t pid){
    u64 *read_ts,ts;
    read_ts = bpf_map_lookup_elem(&read_time_map, &pid);
    if (!read_ts){
        return 0;
    }
    ts = *read_ts;
    return ts;
}

static int get_org_fd(uint32_t pid){
    u64 read_ts = get_readts(pid);
    if (read_ts){
        struct r_fd *orig_fd;
        orig_fd = bpf_map_lookup_elem(&request_fd, &pid);
        if (orig_fd)
            return orig_fd->fd;
    }
    return 0;
}

static void handle_read(uint64_t id,int fd, void *ctx){
    // 新建请求，会将该pid之前的未write的请求抹去
    u64 start_ts=bpf_ktime_get_ns();
    u64 read_ts = start_ts;

    uint32_t pid = id;
    delete_unwrite_pid(pid);

    bpf_map_update_elem(&start_on, &pid, &start_ts, 0);
    bpf_map_update_elem(&read_time_map, &pid, &read_ts,0);

    struct read_key r_k={};
    r_k.read_ts = read_ts;
    struct val_t_on val;
    __builtin_memset(&val, 0, sizeof(val));
    val.delta = 0;
    val.flag = 0;
    bpf_map_update_elem(&info_on,&r_k,&val,BPF_NOEXIST);

    struct stacks_q new_stack={};
    new_stack.kern_stack_id = 0;
    new_stack.read_ts = read_ts;
    bpf_map_update_elem(&request_head, &read_ts, &new_stack,BPF_NOEXIST);

    struct r_fd orig_fd={};
    orig_fd.fd = fd;
    bpf_map_update_elem(&request_fd, &pid, &orig_fd,0);

}


static int write_record(uint64_t id, void *ctx){
    // 记录请求响应
    uint32_t pid = id;
    u64 *start_ts,*read_ts;
    start_ts = bpf_map_lookup_elem(&start_on, &pid);
    if (!start_ts)
        goto cleanup;  
    read_ts = bpf_map_lookup_elem(&read_time_map, &pid);
    if (!read_ts)
        goto cleanup;  
    s64 delta, rtlatency;  
    u64 now_ts = bpf_ktime_get_ns();
    delta = (s64)(now_ts - *start_ts);
    struct read_key r_k={};
    struct val_t_on *valp;
    r_k.read_ts = *read_ts;
    delta = (u64)delta/1000U;
    rtlatency = (s64)(now_ts-*read_ts);
    rtlatency = (u64)rtlatency/1000U;
    valp = bpf_map_lookup_elem(&info_on, &r_k);
    if (!valp)
    {
        goto cleanup;  
    }
    __sync_fetch_and_add(&valp->delta, delta);
    __sync_fetch_and_add(&valp->flag, 1);
    __sync_fetch_and_add(&valp->rtlatency, rtlatency);

cleanup:
    bpf_map_delete_elem(&read_time_map, &pid);
	bpf_map_delete_elem(&start_on, &pid);
  return 0;
}

static void handle_write(uint64_t id, int fd, void *ctx){
    uint32_t pid = id;
    struct r_fd *orig_fd;
    orig_fd = bpf_map_lookup_elem(&request_fd,&pid);
    if (!orig_fd){  
        return;
    }
    if (orig_fd->fd == fd){
        write_record(id,ctx);
        bpf_map_delete_elem(&request_fd,&pid);
        bpf_map_delete_elem(&send_sockfd,&pid);
    }
}

/* server端收到请求 */
static void handle_server_read(u64 read_ts, void *ctx){

    u64 now_ts = bpf_ktime_get_ns();
    bpf_map_update_elem(&server_fd_time, &read_ts, &now_ts, 0);

}

/* server端处理请求 */
static void handle_server_write(u64 read_ts, void *ctx){

    u64 *start_ts;
    start_ts = bpf_map_lookup_elem(&server_fd_time, &read_ts);

    if (start_ts){
        u64 delta;
        delta = (s64)(bpf_ktime_get_ns() - *start_ts);
        delta = (u64)delta/1000U;
        struct read_key r_k = {};
        r_k.read_ts = read_ts;
        struct val_t_on *valp_on;
        valp_on = bpf_map_lookup_elem(&info_on, &r_k);
        if (!valp_on){
            goto cleanup;
        }
        __sync_fetch_and_add(&valp_on->server_delta, delta);

    }
cleanup:
    bpf_map_delete_elem(&server_fd_time, &read_ts);
  
}

struct sys_enter_read_args {
	struct trace_entry ent;
    long __syscall_nr;
	long fd;
    long buf;
    long count;
	char __data[0];
};


SEC("tp/syscalls/sys_enter_read")
// ssize_t read(int fd, void *buf, size_t count);
int tp__sys_enter_read(struct sys_enter_read_args *ctx)
{
    uint64_t id = bpf_get_current_pid_tgid();
    if (!allow_record(id)) {
        goto server_process;
    }
    struct data_param_t read_param = {};
    read_param.syscall_func = FuncRead;
    read_param.fd = (int)ctx->fd;
    // bpf_probe_read(&read_param.fd, sizeof(read_param.fd), &ctx->fd);
    read_param.buf = (const char*)ctx->buf;
    bpf_map_update_elem(&read_param_map, &id, &read_param, BPF_ANY);

    return 0;

server_process:
    /* server端是否能匹配到从connaddr_map来的请求 */
    if (is_server_pid(id)){
        int sockfd;
        sockfd = (int)ctx->fd;
        handle_server_read(sockfd, ctx);
    }
    return 0;
}


SEC("tracepoint/syscalls/sys_exit_read")
int tp_sys_exit_read(struct trace_event_raw_sys_exit *ctx) {
    uint64_t id = bpf_get_current_pid_tgid();
    if (!allow_record(id) && !is_server_pid(id)) {
        return 0;
    }        
    uint32_t pid = id;
    
    struct data_param_t *read_param = bpf_map_lookup_elem(&read_param_map, &id);
    if (read_param != NULL && read_param->real_conn && read_param->buf ) {
        if (allow_record(id)){
            int *sock_fd;
            sock_fd = bpf_map_lookup_elem(&send_sockfd,&pid);
            if (sock_fd && *sock_fd==read_param->fd){
                // bpf_map_delete_elem(&send_sockfd,&pid);
            }else{
                handle_read(id,read_param->fd,ctx);
            }
        }

    }
    bpf_map_delete_elem(&read_param_map, &id);
    return 0;

    return 0;
}

struct sys_enter_write_args {
	struct trace_entry ent;
    long __syscall_nr;
	long  fd;
    long *buf;
    long count;
	char __data[0];
};

SEC("tp/syscalls/sys_enter_write")
int tp_sys_enter_write(struct sys_enter_write_args *ctx) 
{
    uint64_t id = bpf_get_current_pid_tgid();

    if (!allow_record(id))
        return 0;

    struct data_param_t write_param = {};
    write_param.syscall_func = FuncWrite;
    write_param.fd = (int)ctx->fd;
    write_param.iovlen = (size_t)ctx->count;
    bpf_map_update_elem(&write_param_map, &id, &write_param, BPF_ANY);
    return 0;
}


SEC("tracepoint/syscalls/sys_exit_write")
int tp_sys_exit_write(struct trace_event_raw_sys_exit *ctx) {
    uint64_t id = bpf_get_current_pid_tgid();
    if (!allow_record(id)) {
        return 0;
    }        

    struct data_param_t *write_param = bpf_map_lookup_elem(&write_param_map, &id);
    if (write_param != NULL && write_param->real_conn && write_param->iovlen) {
        handle_write(id,write_param->fd,ctx);
    }
    bpf_map_delete_elem(&write_param_map, &id);

    return 0;
}


/*从socket读数据*/
SEC("kprobe/security_socket_recvmsg")
// int security_socket_recvmsg(struct socket *sock, struct msghdr *msg, int size)
int BPF_KPROBE(kprobe_security_socket_recvmsg, struct socket *sock, void *msg, int size)
{
    uint64_t id = bpf_get_current_pid_tgid();
    uint32_t pid = id;
    if (!allow_record(id)) {
        goto server_process;
    }        

    struct data_param_t *read_param = bpf_map_lookup_elem(&read_param_map, &id);
    if (read_param != NULL) {
        read_param->real_conn = true;
    }

server_process:
    if (!is_server_pid(id))
        return 0;
    struct sock *psk= (struct sock *)_(sock->sk);
    struct sock_common sk_c;
    bpf_probe_read(&sk_c, sizeof(struct sock_common), &psk->__sk_common);
    u16 dport = sk_c.skc_dport;     //远端
    int port = __builtin_bswap16(dport);
    u64 *read_ts;
    read_ts = bpf_map_lookup_elem(&port_readts_map, &port);
    if (!read_ts)
        return 0;
    handle_server_read(*read_ts,ctx);

    return 0;
}

SEC("kprobe/security_socket_sendmsg")
int BPF_KPROBE(kprobe_security_socket_sendmsg, struct socket *sock, void *msg, int size)
{
    uint64_t id = bpf_get_current_pid_tgid();
    if (!allow_record(id) && !is_server_pid(id))
        return 0;

    uint32_t pid = id;
    struct data_param_t *write_param = bpf_map_lookup_elem(&write_param_map, &id);
    if (write_param != NULL) {
        write_param->real_conn = true;
    }
    u64 read_ts;

    struct sock *psk= (struct sock *)_(sock->sk);
    struct sock_common sk_c;
    bpf_probe_read(&sk_c, sizeof(struct sock_common), &psk->__sk_common);
    u16 sport = sk_c.skc_num;
    u16 dport = sk_c.skc_dport;

    int fd = get_org_fd(pid);
    read_ts = get_readts(pid);
    if (fd && write_param && fd != write_param->fd){
        bpf_map_update_elem(&port_readts_map, &sport, &read_ts, 0);
        bpf_map_update_elem(&send_sockfd, &pid, &write_param->fd,0);
    }

server_process:
    if (!is_server_pid(id))
        return 0;
    u64 *pread_ts;
    dport =  __builtin_bswap16(dport);
    pread_ts = bpf_map_lookup_elem(&port_readts_map, &dport);
    if (pread_ts){
        handle_server_write(*pread_ts, ctx);
        bpf_map_delete_elem(&port_readts_map, &dport);
    }

    return 0;
}

struct sys_enter_connect_args {
    struct trace_entry ent;
    long __syscall_nr;
    long fd;
    struct sockaddr *uservaddr;
    long addrlen;
	char __data[0];
};

SEC("tp/syscalls/sys_enter_connect")
int tp_sys_enter_connect(struct sys_enter_connect_args *ctx)
{
    uint64_t id = bpf_get_current_pid_tgid();
    uint32_t pid = id;
    if (allow_record(id)) {
        int fd = (int)ctx->fd;
        bpf_map_update_elem(&send_sockfd, &pid, &fd,0);
    }
    return 0;
}

struct sys_enter_sendmsg_args {
    struct trace_entry ent;
    long __syscall_nr;
    long fd;
    struct user_msghdr * msg;
    long flags;
	char __data[0];
};

SEC("tp/syscalls/sys_enter_sendmsg")
// ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags);
int tp_sys_enter_sendmsg(struct sys_enter_sendmsg_args *ctx)
{
    uint64_t id = bpf_get_current_pid_tgid();
    uint32_t pid = id;
    int sockfd = (int)ctx->fd;
    if (allow_record(id)) {        
        struct data_param_t write_param = {};
        write_param.syscall_func = FuncSendMsg;
        write_param.fd = sockfd;
        bpf_map_update_elem(&write_param_map, &id, &write_param, BPF_ANY);
    }

    return 0;
}

SEC("tracepoint/syscalls/sys_exit_sendmsg")
int tp_sys_exit_sendmsg(struct trace_event_raw_sys_exit *ctx)
{
    uint64_t id = bpf_get_current_pid_tgid();
    uint32_t pid = id;
    if (allow_record(id)) {
        struct data_param_t *write_param = bpf_map_lookup_elem(&write_param_map, &id);
        if (write_param != NULL) {
            struct r_fd *orig_fd;
            orig_fd = bpf_map_lookup_elem(&request_fd,&pid);
            if (orig_fd && write_param->fd == orig_fd->fd){
                handle_write(id,write_param->fd,ctx);
            }
        }
        bpf_map_delete_elem(&write_param_map, &id);
    }
    return 0;
}

struct sys_enter_recvmsg_args {
    struct trace_entry ent;
    long __syscall_nr;
    long fd;
    struct user_msghdr * msg;
    long flags;
	char __data[0];
};

SEC("tp/syscalls/sys_enter_recvmsg")
// ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags);
int tp_sys_enter_recvmsg(struct sys_enter_recvmsg_args *ctx)
{
    uint64_t id = bpf_get_current_pid_tgid();
    if (allow_record(id)) {
        struct data_param_t read_param = {};
        read_param.syscall_func = FuncRecvMsg;
        read_param.fd = (int)ctx->fd;
        bpf_map_update_elem(&read_param_map, &id, &read_param, BPF_ANY);
    }    
    if (is_server_pid(id)){
        int sockfd = (int)ctx->fd;
        handle_server_read(sockfd, ctx);
    }
    return 0;    
}

SEC("tracepoint/syscalls/sys_exit_recvmsg")
int tp_sys_exit_recvmsg(struct trace_event_raw_sys_exit *ctx)
{
    uint64_t id = bpf_get_current_pid_tgid();
    uint32_t pid = id;
    if (allow_record(id)) {
        struct data_param_t *read_param = bpf_map_lookup_elem(&read_param_map, &id);
        if (read_param != NULL) {
            int *sock_fd;
            sock_fd = bpf_map_lookup_elem(&send_sockfd,&pid);
            if (sock_fd && *sock_fd==read_param->fd){
                // bpf_map_delete_elem(&send_sockfd,&pid);
            }else{
                handle_read(id, read_param->fd, ctx);
            }
        }
        bpf_map_delete_elem(&read_param_map, &id);
    }
    return 0;
}

struct sys_enter_sendto_args {
    struct trace_entry ent;
    long __syscall_nr;
    long fd;
    long buff;
    long len;
    long flags;
    struct sockaddr *addr;
    long addr_len;
	char __data[0];
};

SEC("tp/syscalls/sys_enter_sendto")
// ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
//                const struct sockaddr *dest_addr, socklen_t addrlen);
int tp_sys_enter_sendto(struct sys_enter_sendto_args *ctx)
{
    uint64_t id = bpf_get_current_pid_tgid();
    uint32_t pid = id;
    int sockfd = (int)ctx->fd;

    if (allow_record(id)) {
        struct data_param_t write_param = {};
        write_param.syscall_func = FuncSendTo;
        write_param.fd = sockfd;
        // write_param.buf = (const char *)_(ctx->buff);
        bpf_map_update_elem(&write_param_map, &id, &write_param, BPF_ANY);
    }    

    return 0;
}

SEC("tracepoint/syscalls/sys_exit_sendto")
int tp_sys_exit_sendto(struct trace_event_raw_sys_exit *ctx)
{
    uint64_t id = bpf_get_current_pid_tgid();
    uint32_t pid = id;
    if (allow_record(id)) {
        struct data_param_t *write_param = bpf_map_lookup_elem(&write_param_map, &id);
        if (write_param != NULL) {

            struct r_fd *orig_fd;
            orig_fd = bpf_map_lookup_elem(&request_fd,&pid);
            if (orig_fd && write_param->fd == orig_fd->fd){
                handle_write(id,write_param->fd,ctx);
            }
        }
        bpf_map_delete_elem(&write_param_map, &id);
    }
    return 0;
}

struct sys_enter_recvfrom_args {
    struct trace_entry ent;
    long __syscall_nr; 
    long fd;
    long ubuf;
    long size;
    long flags;
    long addr;
    long addr_len;
	char __data[0];
};

SEC("tp/syscalls/sys_enter_recvfrom")
// ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
//                  struct sockaddr *src_addr, socklen_td *addrlen);
int tp_sys_enter_recvfrom(struct sys_enter_recvfrom_args *ctx)
{
    uint64_t id = bpf_get_current_pid_tgid();

    if (allow_record(id)) {
        struct data_param_t read_param = {};
        read_param.syscall_func = FuncRecvFrom;
        read_param.fd = (int)ctx->fd;
        read_param.buf = (const char *)ctx->ubuf;
        bpf_map_update_elem(&read_param_map, &id, &read_param, BPF_ANY);
    }    
    if (is_server_pid(id)){
        int sockfd = (int)ctx->fd;
        handle_server_read(sockfd, ctx);
    }

    return 0;
}

SEC("tracepoint/syscalls/sys_exit_recvfrom")
int tp_sys_exit_recvfrom(struct  trace_event_raw_sys_exit *ctx)
{
  uint64_t id = bpf_get_current_pid_tgid();
  uint32_t pid = id;
  struct data_param_t *read_param = bpf_map_lookup_elem(&read_param_map, &id);
  if (read_param != NULL && read_param->real_conn && read_param->buf) {
        if (allow_record(id)){
            int *sock_fd;
            sock_fd = bpf_map_lookup_elem(&send_sockfd,&pid);
            if (sock_fd && *sock_fd==read_param->fd){
                // bpf_map_delete_elem(&send_sockfd,&pid);
            }else{
                handle_read(id,read_param->fd,ctx);
            }

        }

  }
  bpf_map_delete_elem(&read_param_map, &id);
  return 0;

}

struct sched_switch_tp_args {
	struct trace_entry ent;
	char prev_comm[16];
	pid_t prev_pid;
	int prev_prio;
	long int prev_state;
	char next_comm[16];
	pid_t next_pid;
	int next_prio;
	char __data[0];
};

SEC("tp/sched/sched_switch")
// int tp__sched_switch(struct sched_switch_tp_args *ctx)
int tp__sched_switch(struct sched_switch_tp_args *ctx)
{
    struct task_struct *prev = (void *)bpf_get_current_task();
    uint32_t prev_pid = _(prev->pid), prev_tgid = _(prev->tgid);
    uint32_t next_pid;

    struct perf_oncpu p_o={};
    u64 *start_ts, *read_ts, *read_ts_next, *start_rq;
    u64 now_ts = bpf_ktime_get_ns();
    struct val_t val,*valp;
    struct val_t_on *valp_on;
    s64 delta;  
    struct read_key r_k={};

    if (allow_record_tgid(prev_tgid))
    {
            
        if (prev_pid == 0)
		    prev_pid = bpf_get_smp_processor_id();

        // record oncpu time
        start_ts = bpf_map_lookup_elem(&start_on, &prev_pid);
        if (!start_ts)
            goto next_pid_process;

        delta = (s64)(now_ts - *start_ts);

        read_ts = bpf_map_lookup_elem(&read_time_map, &prev_pid);
        if (!read_ts)
        {
            bpf_map_delete_elem(&start_on, &prev_pid);  
            goto next_pid_process;
        }        

        r_k.read_ts = *read_ts;
        delta = (u64)delta/1000U;
        valp_on = bpf_map_lookup_elem(&info_on, &r_k);
        if (!valp_on){
            bpf_map_delete_elem(&start_on, &prev_pid);  
            goto next_pid_process;
        }
        __sync_fetch_and_add(&valp_on->delta, delta);

        bpf_map_delete_elem(&start_on, &prev_pid);  

        // record offcpu stacks
        struct internal_key_off i_key={};
        i_key.start_ts = now_ts;
        i_key.key.read_ts = *read_ts;
        /* There have be a bug in linux-4.19 for bpf_get_stackid in raw_tracepoint */
        i_key.key.kern_stack_id = bpf_get_stackid(ctx, &stackmap, KERN_STACKID_FLAGS);

        if (!bpf_map_lookup_elem(&info_off, &i_key.key))
        {
            __builtin_memset(&val, 0, sizeof(val));
            // bpf_probe_read_str(&val.comm, sizeof(prev->comm), prev->comm);
            val.delta = 0;
            bpf_map_update_elem(&info_off, &i_key.key, &val, BPF_NOEXIST);
            struct stacks_q new_head={}, *old_head;
            old_head = bpf_map_lookup_elem(&request_head, &r_k.read_ts);
            if (!old_head)
                goto next_pid_process;
            new_head.kern_stack_id = i_key.key.kern_stack_id;
            new_head.read_ts = *read_ts;
            bpf_map_update_elem(&request_stacks, &new_head, old_head, BPF_NOEXIST);
            bpf_map_update_elem(&request_head, read_ts, &new_head, 0);
        }
        bpf_map_update_elem(&start_off, &prev_pid, &i_key, 0);
        bpf_map_update_elem(&start_runqueue,&prev_pid,&now_ts,0);
    }

next_pid_process:
    next_pid = ctx->next_pid;
    if (1)
    {
        // 只有进入了read的thread才需要记录
        read_ts_next = bpf_map_lookup_elem(&read_time_map, &next_pid);
        if (!read_ts_next)
            return 0;

        bpf_map_update_elem(&start_on, &next_pid, &now_ts, 0);

        start_rq = bpf_map_lookup_elem(&start_runqueue, &next_pid);
        if (!start_rq)
            return 0;
        delta = (s64)(now_ts - *start_rq);
        delta = (u64)delta/1000U;
        r_k.read_ts = *read_ts_next;
        valp_on = bpf_map_lookup_elem(&info_on, &r_k);
        if (!valp_on)
            goto cleanup;
        
        __sync_fetch_and_add(&valp_on->runqueue, delta);

    }
cleanup:
    bpf_map_delete_elem(&start_runqueue, &next_pid);  
    return 0;
}

struct sched_wakeup_tp_args {
	struct trace_entry ent;
	char comm[16];
	pid_t pid;
	int prio;
	int success;
	int target_cpu;
	char __data[0];
};

static int wakeup(void *ctx, pid_t pid)
{
    u64 now_ts = bpf_ktime_get_ns();

    // 记录off时间
    struct internal_key_off *i_keyp;
    i_keyp = bpf_map_lookup_elem(&start_off, &pid);
    if (!i_keyp){
        goto cleanup;
    }
    s64 delta;
    struct val_t *valp;

    delta = (s64)(now_ts- i_keyp->start_ts);
    if (delta < 0)
    {
        goto cleanup;
    }
    delta = (u64)delta/1000U;
    valp = bpf_map_lookup_elem(&info_off, &i_keyp->key);
    if (!valp)
    {
        goto cleanup;
    }
    __sync_fetch_and_add(&valp->delta, delta);

    // 记录进入runqueue的时间戳
    bpf_map_update_elem(&start_runqueue,&pid,&now_ts,0);

cleanup:
    bpf_map_delete_elem(&start_off, &pid);

	return 0;
}


SEC("tp/sched/sched_wakeup")
int tp__sched_wakeup(struct sched_wakeup_tp_args *ctx)
{
    pid_t pid = 0;
	bpf_probe_read(&pid, sizeof(pid), &(ctx->pid));
    wakeup(ctx, pid);
    return 0;
}
