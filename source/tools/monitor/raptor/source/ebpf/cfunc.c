/*
 * Author: Chen Tao
 * Create: Mon Sep 19 19:23:35 2022
 */

#define _GNU_SOURCE
#include "dlfcn.h"
#include "string.h"
#include "stdio.h"
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include "net.h"
#include <getopt.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
/*
 * test result:
 *
 * net init end...
 * success to update map for tgid: -1
 * success to update map for protocol: 1
 * net config end...
 * net start end...
 * fuc:1(sys_enter_connect), 0.0.0.0:0-> 127.0.0.1:54046: size = 0, com_name:client, pid: 93416, fd:3, family:2, ret_val:0
 * fuc:1(sys_enter_connect), 0.0.0.0:0-> 127.0.0.1:54046: size = 0, com_name:client, pid: 93462, fd:3, family:2, ret_val:0
 * fuc:3(sys_enter_accept), 0.0.0.0:0-> 0.0.0.0:0: size = 0, com_name:server, pid: 93521, fd:3, family:2236, ret_val:0)))
 */

#define MAX_PROTOCOL_NUM 5

const char* dll_path = "./libnet.so";
#define MAP_DATA_SIZE 5

struct env_para_t {
	int proto;
	int pid;
	int self_pid;
	int port;
	int data_sample;
	int ctrl_count;
	int data_count;
	int stat_count;
	int debug;
	FILE * file;
	char instance_id[64];
};

struct env_para_t env_para = {
	.proto = -1,
	.pid = -1,
	.self_pid = -1,
	.port = -1,
	.data_sample = -1,
	.ctrl_count = 0,
	.data_count = 0,
	.stat_count = 0,
	.debug = 0,
	.file = NULL,
};

enum libbpf_print_level {
	DEBUG = 0,	
	WARN,
	INFO,
	ERROR,
	MAX,
};
typedef void (*net_data_process_func_t)(void * custom_data, struct conn_data_event_t * event);
typedef void (*net_ctrl_process_func_t)(void * custom_data, struct conn_ctrl_event_t * event);
typedef void (*net_statistics_process_func_t)(void * custom_data, struct conn_stats_event_t* event);
typedef int (*net_print_fn_t)(enum libbpf_print_level level, const char *format, va_list args);

typedef void (*d_ebpf_setup_net_data_process_func)(net_data_process_func_t func, void * custom_data);
typedef void (*d_ebpf_setup_net_event_process_func)(net_ctrl_process_func_t func, void * custom_data);
typedef void (*d_ebpf_setup_net_statistics_process_func)(net_statistics_process_func_t func, void * custom_data);
typedef void (*d_ebpf_setup_print_func)(net_print_fn_t func);
typedef int32_t (*d_ebpf_poll_events)(int32_t max_events, int32_t *stop_flag);
typedef int32_t (*d_ebpf_init)(char *btf, int32_t btf_size, char *so, int32_t so_size, long uprobe_offset);
typedef int32_t (*d_ebpf_start)(void);
typedef int32_t (*d_ebpf_get_map_value)(int fd, const void *key, void *value);
typedef int32_t (*d_ebpf_stop)(void);
typedef int32_t (*d_ebpf_get_fd)(void);
typedef int32_t (*d_ebpf_get_proto_fd)(int proto);
typedef int32_t (*d_ebpf_get_next_key)(int32_t fd, const void *key, void *next_key);
typedef void (*d_ebpf_delete_map_value)(void *key, int32_t size);
typedef void (*d_ebpf_config)(int32_t opt1, int32_t opt2, int32_t params_count, void ** params, int32_t * params_len);
typedef void (*d_ebpf_cleanup_dog)(void *key, int32_t size);

d_ebpf_config g_ebpf_config_func = NULL;
d_ebpf_get_fd ebpf_get_fd = NULL;
d_ebpf_get_proto_fd ebpf_get_proto_fd = NULL;
d_ebpf_get_next_key ebpf_get_next_key = NULL;
d_ebpf_delete_map_value ebpf_delete_map_value = NULL;
d_ebpf_get_map_value ebpf_get_map_value = NULL;
d_ebpf_setup_net_data_process_func ebpf_setup_net_data_process_func = NULL;
d_ebpf_poll_events ebpf_poll_events = NULL;
d_ebpf_setup_net_event_process_func ebpf_setup_net_event_process_func = NULL;
d_ebpf_setup_net_statistics_process_func ebpf_setup_net_statistics_process_func = NULL;
d_ebpf_init ebpf_init = NULL;
d_ebpf_start ebpf_start = NULL;
d_ebpf_stop ebpf_stop = NULL;
d_ebpf_config ebpf_config = NULL;
d_ebpf_cleanup_dog ebpf_cleanup_dog = NULL;

int cgo_ebpf_set_config(int opt, int value)
{
	if (ebpf_config == NULL)
		return -1;
	int32_t * params[] = {&value};
	int32_t Len[] = {4};
	ebpf_config(opt, 0, 1, (void **)params, Len);
}

int cgo_poll_events(int max_event, int * flag)
{
	return ebpf_poll_events(max_event, flag);
}

int cgo_ebpf_setup_net_data_process_func(net_data_process_func_t func)
{
	ebpf_setup_net_data_process_func(func, NULL);
}

int cgo_ebpf_setup_net_event_process_func(net_ctrl_process_func_t func)
{
    ebpf_setup_net_event_process_func(func, NULL);
}

int cgo_ebpf_setup_net_statistics_process_func(net_statistics_process_func_t func)
{
    ebpf_setup_net_statistics_process_func(func, NULL);
}

int cgo_ebpf_config(int32_t opt1, int32_t opt2, int32_t params_count, void ** params, int32_t * params_len)
{
    ebpf_config(opt1, opt2, params_count, params, params_len);
}

int cgo_ebpf_init(char *btf, int32_t btf_size, char *so, int32_t so_size, long uprobe_offset)
{
    return ebpf_init(btf, btf_size, so, so_size, uprobe_offset);
}

int cgo_ebpf_stop(void)
{
    return ebpf_stop();
}

int cgo_ebpf_get_fd(void)
{
    return ebpf_get_fd();
}

int cgo_ebpf_get_proto_fd(int proto)
{
    return ebpf_get_proto_fd(proto);
}

int cgo_ebpf_get_next_key(int32_t fd, const void *key, void *next_key)
{
    return ebpf_get_next_key(fd, key, next_key);
}

struct ip_info cgo_ebpf_addr2ip_info(union sockaddr_t *addr)
{
	struct ip_info ip = {};
	if (addr->sa.sa_family == AF_INET) {
		ip.port = ntohs(addr->in4.sin_port);
		inet_ntop(AF_INET, &addr->in4.sin_addr, ip.ip, sizeof(ip.ip));
	} else if (addr->sa.sa_family == AF_INET6) {
		ip.port = ntohs(addr->in6.sin6_port);
		inet_ntop(AF_INET6, &addr->in6.sin6_addr, ip.ip, sizeof(ip.ip));
	}
	ip.ip_len = strlen(ip.ip);
	return ip;
}

uint64_t cgo_ebpf_get_map_value(int32_t fd, int key)
{
	uint64_t value = 0;

	ebpf_get_map_value(fd, &key, &value);
	return value;
}

void cgo_ebpf_delete_map_value(void *key, int32_t size)
{
    return ebpf_delete_map_value(key, size);
}

void cgo_ebpf_cleanup_dog(void *key, int32_t size)
{
    return ebpf_cleanup_dog(key, size);
}

static int print_callback(enum libbpf_print_level level,
		const char *format, va_list args)
{
	int ret;
	if (env_para.debug) {
		ret = vfprintf(stderr, format, args);
	}
	return ret;
}

int cgo_env_init(void)
{
	int err;
	int stop_flag = 0;
	void *handle;
	char so_file[64] = "./libnet.so";
	char btf_file[128] = "/usr/lib/vmlinux-4.19.91-007.ali4000.alios7.x86_64";
	Dl_info dlinfo;
	int i;
	int fd;

   	handle = dlopen(dll_path, RTLD_LAZY);
	if (!handle) {
		printf("load so failed,%s\n", dlerror());
		return -1;
	}

	d_ebpf_setup_print_func ebpf_setup_print_func = dlsym(handle, "ebpf_setup_print_func");

	ebpf_setup_net_event_process_func = dlsym(handle, "ebpf_setup_net_event_process_func");
	ebpf_setup_net_data_process_func = dlsym(handle, "ebpf_setup_net_data_process_func");

	ebpf_setup_net_statistics_process_func = dlsym(handle, "ebpf_setup_net_statistics_process_func");
	ebpf_poll_events = (d_ebpf_poll_events)dlsym(handle, "ebpf_poll_events");
	ebpf_init = (d_ebpf_init)dlsym(handle, "ebpf_init");
	ebpf_start = (d_ebpf_start)dlsym(handle, "ebpf_start");
	ebpf_stop = (d_ebpf_stop)dlsym(handle, "ebpf_stop");
	ebpf_config = (d_ebpf_config)dlsym(handle, "ebpf_config");
	ebpf_get_fd = (d_ebpf_get_fd)dlsym(handle, "ebpf_get_fd");
	ebpf_get_proto_fd = (d_ebpf_get_proto_fd)dlsym(handle, "ebpf_get_proto_fd");
	ebpf_get_next_key = (d_ebpf_get_next_key)dlsym(handle, "ebpf_get_next_key");
	ebpf_delete_map_value = (d_ebpf_delete_map_value)dlsym(handle, "ebpf_delete_map_value");
	ebpf_cleanup_dog = (d_ebpf_cleanup_dog)dlsym(handle, "ebpf_cleanup_dog");
	ebpf_get_map_value = (d_ebpf_get_map_value)dlsym(handle, "ebpf_get_map_value");
	err = dladdr(ebpf_cleanup_dog, &dlinfo);
	if (err) {
		printf("laddr failed, err:%s\n", strerror(err));
	}
	long uprobe_offset = (long)dlinfo.dli_saddr - (long)dlinfo.dli_fbase;

	printf("uprobe offset:%x\n", uprobe_offset);
	/* must set the print callback */
	ebpf_setup_print_func(print_callback);
	err = ebpf_init(NULL, 0, so_file, strlen(so_file), uprobe_offset);
	if (err) {
		printf("init failed, err:%d\n", err);
		return err;
	}
	printf("input para pid:%d, proto:%d, self:%d, sample:%d, port:%d, debug:%d\n",
			env_para.pid, env_para.proto, env_para.self_pid, env_para.data_sample, env_para.port, env_para.debug);

	printf("net init end...\n");
	
	err = ebpf_start();
	if (err) {
		printf("start failed, err:%d\n", err);
		ebpf_stop();
		return 0;
	}
	printf("net start end...\n");

	return 0;
}
