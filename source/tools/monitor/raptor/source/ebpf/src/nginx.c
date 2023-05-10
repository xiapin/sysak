/*
 * Author: Chen Tao
 * Create: Mon Nov 14 11:58:46 2022
 */
#include <stdio.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/resource.h>
#include <arpa/inet.h>
#include <bpf/libbpf.h>
#include "uprobe_helpers.h"
#include <errno.h>
#include "nginx.h"
#include "nginx.skel.h"

#define UPROBE_ATTACH(probe_name, skel, elf_path, func_name, ret) \
    do { \
        int e; \
        long long offset; \
        offset = get_elf_func_offset(elf_path, #func_name); \
        if (offset < 0) { \
            fprintf(stderr, "Failed to get func(" #func_name ") in(%s) offset.\n", elf_path); \
            ret = 0; \
            break; \
        } \
        probe_name##_link[probe_name##_link_current] = bpf_program__attach_uprobe( \
            skel->progs.func_name##_fn, false /* not uretprobe */, -1, elf_path, (size_t)offset); \
        e = libbpf_get_error(probe_name##_link[probe_name##_link_current]); \
        if (e) { \
            fprintf(stderr, "Failed to attach uprobe(" #func_name "): %d\n", e); \
            ret = 0; \
            break; \
        } \
        fprintf(stdout, "Success to attach uprobe(" #probe_name "): to elf: %s\n", elf_path); \
        probe_name##_link_current += 1; \
        ret = 1; \
    } while (0)

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
	return vfprintf(stderr, format, args);
}

static void bump_memlock_rlimit(void)
{
	struct rlimit rlim_new = {
		.rlim_cur	= RLIM_INFINITY,
		.rlim_max	= RLIM_INFINITY,
	};

	if (setrlimit(RLIMIT_MEMLOCK, &rlim_new)) {
		fprintf(stderr, "Failed to increase RLIMIT_MEMLOCK limit!\n");
		exit(1);
	}
}

static int ip_format(struct ip_addr *ip, char *ipstr, int size)
{
	char tmp[32] = {0};

	if (ip->family == AF_INET) {
		inet_ntop(AF_INET, &ip->ipaddr.ip4, tmp, 16);
	} else {
		inet_ntop(AF_INET6, &ip->ipaddr.ip6, tmp, 32);
	}
	snprintf(ipstr, size, "%s", tmp);
	return 0;
}

static void handle_event(void *ctx, int cpu, void *data, __u32 data_sz)
{
	struct ngx_trace *e = (struct ngx_trace *)data;
	struct tm *tm;
	char ts[16];
	time_t t;
	char cip[32] = {0};

	time(&t);
	tm = localtime(&t);
	strftime(ts, sizeof(ts), "%H:%M:%S", tm);
	if (e->nw.exit_cnt) {
		printf("time:%s, trace cpu:%d, pid:%d, exit_cnt:%d\n", ts, e->nw.cpu, e->nw.pid, e->nw.exit_cnt);
	}
	ip_format(&e->srcip, cip, sizeof(cip));
	printf("client ip:%s\n", cip);
}

static void handle_lost_events(void *ctx, int cpu, __u64 data_sz)
{
	printf("lost data\n");
}

int nginx_ebpf_init()
{
	struct nginx_bpf *skel;
	int err, i;
	struct perf_buffer *pb = NULL;
	struct bpf_link *nginx_link[32] = {};
    int nginx_link_current = 0;

	struct perf_buffer_opts pb_opts = {
		.sample_cb = handle_event,
		.lost_cb = handle_lost_events,
	};

	bump_memlock_rlimit();
	libbpf_set_print(libbpf_print_fn);
	DECLARE_LIBBPF_OPTS(bpf_object_open_opts, open_opts);
	open_opts.btf_custom_path = NULL;

	skel = nginx_bpf__open_opts(&open_opts);
	if (!skel) {
		fprintf(stderr, "Failed to open BPF skeleton\n");
		return 1;
	}

	err = nginx_bpf__load(skel);
	if (err) {
		fprintf(stderr, "Failed to load and verify BPF skeleton\n");
		goto cleanup;
	}

    UPROBE_ATTACH(nginx, skel, "/usr/sbin/nginx", ngx_close_connection, err);
    if (!err) {
		fprintf(stderr, "uprobe failed, func: ngx_close_connection, err:%s\n", strerror(err));
        goto cleanup;
    }
    UPROBE_ATTACH(nginx, skel, "/usr/sbin/nginx", ngx_http_create_request, err);
    if (!err) {
		fprintf(stderr, "uprobe failed, func: ngx_http_create_request, err:%s\n", strerror(err));
        goto cleanup;
    }
    err = nginx_bpf__attach(skel);
    if (err) {
        fprintf(stderr, "nginx attach failed:%s\n", strerror(err));
        goto cleanup;
    }

	pb = perf_buffer__new(bpf_map__fd(skel->maps.events_map), 256, &pb_opts);
	while (1) {
		err = perf_buffer__poll(pb, 200);
	}
	for (;;) {
		fprintf(stderr, ".");
		sleep(1);
	}
cleanup:
	perf_buffer__free(pb);
	for (i = 0; i < nginx_link_current; i++) {
		bpf_link__destroy(nginx_link[i]);
	}
	nginx_bpf__destroy(skel);
	return -err;
} 

int ngx_close_connection_offset()
{
	return get_elf_func_offset("/usr/sbin/nginx", "ngx_close_connection");
}

int ngx_http_create_request_offset()
{
	return get_elf_func_offset("/usr/sbin/nginx", "ngx_http_create_request");
}