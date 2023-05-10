/*
 * Author: Chen Tao
 * Create: Mon Sep 19 19:23:40 2022
 */
#ifndef CFUNC_H
#define CFUNC_H

#include "netapi.h"

int cgo_env_init(void);

int cgo_poll_events(int max_events, int *stop_flag);

int cgo_ebpf_setup_net_data_process_func(net_data_process_func_t func);

int cgo_ebpf_setup_net_event_process_func(net_ctrl_process_func_t func);

int cgo_ebpf_setup_net_statistics_process_func(net_statistics_process_func_t func);

int cgo_ebpf_config(int32_t opt1, int32_t opt2, int32_t params_count, void ** params, int32_t * params_len);

int cgo_ebpf_init(char *btf, int32_t btf_size, char *so, int32_t so_size, long uprobe_offset);

int cgo_ebpf_stop(void);

int cgo_ebpf_get_fd(void);

int cgo_ebpf_get_proto_fd(int proto);

int cgo_ebpf_get_next_key(int32_t fd, const void *key, void *next_key);

// return value
uint64_t cgo_ebpf_get_map_value(int32_t fd, int key);

void cgo_ebpf_delete_map_value(void *key, int32_t size);

void cgo_ebpf_cleanup_dog(void *key, int32_t size);

int cgo_ebpf_set_config(int opt, int value);

struct ip_info cgo_ebpf_addr2ip_info(union sockaddr_t *addr);
#endif