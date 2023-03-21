/*
 * Author: Chen Tao
 * Create: Mon Nov 14 13:43:29 2022
 */
#ifndef __NGX_VERSION_H__
#define __NGX_VERSION_H__
#include <asm/types.h>

#ifndef IP6_LEN
#define IP6_LEN 16
#endif

#ifndef AF_INET
#define AF_INET 2
#endif

#ifndef AF_INET6
#define AF_INET6 2
#endif

struct ngx_ip {
    union {
        __u32 ip4;
        unsigned char ip6[IP6_LEN];
    };
};

struct ip_addr {
    struct ngx_ip ipaddr;
    __u16 port;
    __u32 family;
};

struct ngx_worker {
    int pid;
    int cpu;
    int exit_cnt;
    int handle_cnt;
};

// nginx tracing struct
struct ngx_trace {
    struct ip_addr srcip;
    struct ip_addr ngxip;
    char dst_ipstr[48];
    int is_finish;
    struct ngx_worker nw;
};

// nginx struct
typedef struct {
    unsigned int len;
    unsigned char data[32];
} ngx_str_addr_t;

typedef struct {
    unsigned int len;
    unsigned char *data;
} ngx_str_t;

struct ngx_connection_s {
    void *data;
    void *read;
    void *write;
    int fd;
    unsigned char temp1[68];
    int type;
    struct sockaddr *sockaddr;
    unsigned int socklen;
    unsigned char temp2[40];
    struct sockaddr *local_sockaddr;
    unsigned int local_socklen;
    unsigned char temp3[48];
};

struct ngx_peer_connection_s {
    struct ngx_connection_s *connection;
    struct sockaddr *sockaddr;
    unsigned int socklen;
    ngx_str_t *name;
    unsigned char temp[80];
};

struct ngx_stream_upstream_s {
    struct ngx_peer_connection_s peer;
    unsigned char temp[272];
};

struct ngx_stream_session_s {
    unsigned char temp1[8];
    struct ngx_connection_s *connection;
    unsigned char temp2[56];
    struct ngx_stream_upstream_s *upstream;
    unsigned char temp3[64];
};

typedef struct ngx_connection_s ngx_connection_t;

typedef struct ngx_peer_connection_s ngx_peer_connection_t;

struct ngx_event_s {
    void *data;
};
typedef void (*ngx_http_upstream_handler_pt)(void *r, void *u);

struct ngx_http_upstream_s {
    ngx_http_upstream_handler_pt read_event_handler;
    ngx_http_upstream_handler_pt write_event_handler;

    ngx_peer_connection_t peer;
};

typedef void (*ngx_http_event_handler_pt)(void *r);
struct ngx_http_request_s {
    __u32 signature; /* "HTTP" */

    ngx_connection_t *connection;

    void **ctx;
    void **main_conf;
    void **srv_conf;
    void **loc_conf;

    ngx_http_event_handler_pt read_event_handler;
    ngx_http_event_handler_pt write_event_handler;

    void *cache;

    struct ngx_http_upstream_s *upstream;
};

int nginx_ebpf_init();
int ngx_close_connection_offset();
int ngx_http_create_request_offset();
#endif
