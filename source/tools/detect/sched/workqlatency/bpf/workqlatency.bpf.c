#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include "../workqlatency.h"

#define KWORK_COUNT 100
#define MAX_KWORKNAME 128
#define NULL ((void *)0)
#define _(P) ({typeof(P) val = 0; bpf_probe_read(&val, sizeof(val), &P); val;})

struct trace_event_raw_workqueue_activate_work {
        struct trace_entry ent;
        void *work;
        char __data[0];
};

struct trace_event_raw_workqueue_execute_end {
        struct trace_entry ent;
        void *work;
        char __data[0];
};

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(key_size, sizeof(struct work_key));
	__uint(value_size, sizeof(struct report_data));
	__uint(max_entries, KWORK_COUNT);
} runtime_kwork_report SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(key_size, sizeof(struct work_key));
	__uint(value_size, sizeof(struct report_data));
	__uint(max_entries, KWORK_COUNT);
} latency_kwork_report SEC(".maps");


SEC("tp/workqueue/workqueue_execute_start")
int report_workqueue_execute_start(struct trace_event_raw_workqueue_execute_start *ctx)
{
	struct report_data new;
	unsigned long long func_addr;
	struct work_key key = {
		.type = KWORK_CLASS_WORKQUEUE,
		.cpu  = bpf_get_smp_processor_id(),
		.id   = (__u64)_(ctx->work),
	};

	func_addr = (unsigned long long)_(ctx->function);
	__builtin_memset(&new, 0, sizeof(struct report_data));
	new.nr = 0;
	new.total_time = 0;
	new.max_time = 0;
	new.max_time_start = bpf_ktime_get_ns();
	new.max_time_end = 0;
	new.name_addr = func_addr;
	return bpf_map_update_elem(&runtime_kwork_report, &key, &new, BPF_NOEXIST);
}

SEC("tp/workqueue/workqueue_execute_end")
int report_workqueue_execute_end(struct trace_event_raw_workqueue_execute_end *ctx)
{
	__u64 name;
	__s64 delta;
	struct report_data *data;
	struct work_key key = {
		.type = KWORK_CLASS_WORKQUEUE,
		.cpu  = bpf_get_smp_processor_id(),
		.id   = (__u64)_(ctx->work),
	};
	
	data = bpf_map_lookup_elem(&runtime_kwork_report, &key);
	if (data) {
		__u64 now = bpf_ktime_get_ns();
		delta = now - data->max_time_start;
		if (delta < 0)
			return -1;
		if ((delta > data->max_time) || (data->max_time == 0)) {
			data->max_time = delta;
			//data->max_time_start = time_start;
			data->max_time_end = now;
		}
		data->total_time += delta;
		data->nr++;		
	}
	return 0;
}

SEC("tp/workqueue/workqueue_activate_work")
int latency_workqueue_activate_work(struct trace_event_raw_workqueue_activate_work *ctx)
{
	struct report_data new;
	struct work_key key = {
		.type = KWORK_CLASS_WORKQUEUE,
		.cpu  = bpf_get_smp_processor_id(),
		.id   = (__u64)_(ctx->work),
	};

	__builtin_memset(&new, 0, sizeof(struct report_data));
	new.nr = 0;
	new.total_time = 0;
	new.max_time = 0;
	new.max_time_start = bpf_ktime_get_ns();
	new.max_time_end = 0;
	new.name_addr = 0;
	return bpf_map_update_elem(&latency_kwork_report, &key, &new, BPF_NOEXIST);
}

SEC("tp/workqueue/workqueue_execute_start")
int latency_workqueue_execute_start(struct trace_event_raw_workqueue_execute_start *ctx)
{
	__u64 name;
	__s64 delta;
	unsigned long long func_addr;
	struct report_data *data;
	struct work_key key = {
		.type = KWORK_CLASS_WORKQUEUE,
		.cpu  = bpf_get_smp_processor_id(),
		.id   = (__u64)_(ctx->work),
	};

	func_addr = (unsigned long long)_(ctx->function);
	data = bpf_map_lookup_elem(&latency_kwork_report, &key);
	if (data) {
		__u64 now = bpf_ktime_get_ns();
		delta = now - data->max_time_start;
		if (delta < 0)
			return -1;
		if ((delta > data->max_time) || (data->max_time == 0)) {
			data->max_time = delta;
			data->max_time_end = now;
		}
		data->total_time += delta;
		data->nr++;
		data->name_addr = func_addr;
	}
	return 0;
}

char LICENSE[] SEC("license") = "Dual BSD/GPL";