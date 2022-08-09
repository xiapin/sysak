#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_endian.h>

#include "drop.h"
#include "bpf_core.h"

struct tid_map_value
{
	struct sk_buff *skb;
	struct nf_hook_state *state;
	struct xt_table *table;
	u32 hook;
};

struct
{
	__uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
	__uint(key_size, sizeof(u32));
	__uint(value_size, sizeof(u32));
} events SEC(".maps");

struct
{
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 512);
	__type(key, u32);
	__type(value, struct tid_map_value);
} tid_map SEC(".maps");

struct
{
	__uint(type, BPF_MAP_TYPE_STACK_TRACE);
	__uint(max_entries, 1024);
	__uint(key_size, sizeof(u32));
	__uint(value_size, sizeof(uint64_t) * 20);
} stackmap SEC(".maps");

struct
{
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 1);
	__type(key, __u32);
	__type(value, struct filter);
} filter_map SEC(".maps");

#define NF_DROP 0
#define NF_ACCEPT 1

__always_inline void fill_stack(void *ctx, struct event *event)
{
	event->stackid = bpf_get_stackid(ctx, &stackmap, 0);
}

__always_inline void fill_pid(struct event *event)
{
	// pid info
	event->pi.pid = bpf_get_current_pid_tgid() >> 32;
	bpf_get_current_comm(event->pi.comm, sizeof(event->pi.comm));
}

__always_inline void fill_sk_skb(struct event *event, struct sock *sk, struct sk_buff *skb)
{
	struct net *net = NULL;
	struct iphdr ih = {};
	struct tcphdr th = {};
	struct udphdr uh = {};
	u16 protocol = 0;
	bool has_netheader = false;
	u16 network_header, transport_header;
	char *head;
	if (sk)
	{
		// address pair
		bpf_probe_read(&event->skap.daddr, sizeof(event->skap.daddr), &sk->__sk_common.skc_daddr);
		bpf_probe_read(&event->skap.dport, sizeof(event->skap.dport), &sk->__sk_common.skc_dport);
		bpf_probe_read(&event->skap.saddr, sizeof(event->skap.saddr), &sk->__sk_common.skc_rcv_saddr);
		bpf_probe_read(&event->skap.sport, sizeof(event->skap.sport), &sk->__sk_common.skc_num);
		event->skap.dport = bpf_ntohs(event->skap.dport);

		event->sk_protocol = bpf_core_sock_sk_protocol(sk);
		bpf_probe_read(&event->state, sizeof(event->state), &sk->__sk_common.skc_state);
	}

	bpf_probe_read(&head, sizeof(head), &skb->head);
	bpf_probe_read(&network_header, sizeof(network_header), &skb->network_header);
	if (network_header != 0)
	{
		bpf_probe_read(&ih, sizeof(ih), head + network_header);
		has_netheader = true;
		event->ap.saddr = ih.saddr;
		event->ap.daddr = ih.daddr;
		event->protocol = ih.protocol;
		protocol = protocol == 0 ? event->protocol : protocol;
		transport_header = network_header + (ih.ihl << 2);
	}
	else
	{
		bpf_probe_read(&transport_header, sizeof(transport_header), &skb->transport_header);
	}

	if (protocol == 0)
	{
		event->error = ERR_PROTOCOL_NOT_DETERMINED;
		return;
	}

	switch (protocol)
	{
	case IPPROTO_ICMP:
		break;
	case IPPROTO_UDP:
		if (transport_header != 0 && transport_header != 0xffff)
		{
			bpf_probe_read(&uh, sizeof(uh), head + transport_header);
			event->ap.sport = bpf_ntohl(uh.source);
			event->ap.dport = bpf_ntohl(uh.dest);
		}
		break;
	case IPPROTO_TCP:
		if (transport_header != 0 && transport_header != 0xffff)
		{
			bpf_probe_read(&th, sizeof(th), head + transport_header);
			event->ap.sport = bpf_ntohl(th.source);
			event->ap.dport = bpf_ntohl(th.dest);
		}
		break;
	default:
		event->error = ERR_PROTOCOL_NOT_SUPPORT;
		break;
	}
}

__always_inline void handle(void *ctx, struct sock *sk, struct sk_buff *skb)
{
	u32 key = 0;
	struct filter *filter = NULL;
	struct event event = {};

	fill_sk_skb(&event, sk, skb);
	filter = bpf_map_lookup_elem(&filter_map, &key);
	if (filter)
	{
		if (sk)
		{
			// sock addrpair
			if (filter->ap.daddr && event.skap.daddr != filter->ap.daddr)
				return;
			if (filter->ap.saddr && event.skap.saddr != filter->ap.saddr)
				return;
			if (filter->ap.dport && event.skap.dport != filter->ap.dport)
				return;
			if (filter->ap.sport && event.skap.sport != filter->ap.sport)
				return;
		}

		// skb
		if (filter->ap.daddr && event.ap.saddr != filter->ap.daddr)
			return;
		if (filter->ap.saddr && event.ap.daddr != filter->ap.saddr)
			return;
		if (filter->ap.dport && event.ap.sport != filter->ap.dport)
			return;
		if (filter->ap.sport && event.ap.dport != filter->ap.sport)
			return;
	}

	event.type = KFREE_SKB;
	fill_stack(ctx, &event);
	// pid info
	fill_pid(&event);

	bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU, &event, sizeof(event));
}

SEC("kprobe/tcp_drop")
int BPF_KPROBE(tcp_drop, struct sock *sk, struct sk_buff *skb)
{
	handle(ctx, sk, skb);
	return 0;
}

SEC("kprobe/kfree_skb")
int BPF_KPROBE(kfree_skb, struct sk_buff *skb)
{
	struct sock *sk;
	bpf_probe_read(&sk, sizeof(sk), &skb->sk);
	handle(ctx, sk, skb);
	return 0;
}

#define NF_DROP 0

__always_inline void ipt_do_table_entry(struct sk_buff *skb, struct nf_hook_state *state, struct xt_table *table, u32 hook)
{
	u32 tid = bpf_get_current_pid_tgid();
	struct tid_map_value value = {};
	value.skb = skb;
	value.state = state;
	value.table = table;
	value.hook = hook;
	bpf_map_update_elem(&tid_map, &tid, &value, BPF_ANY);
}

// for kernel 4.19 and 5.10
// unsigned int
// ipt_do_table(struct sk_buff *skb,
// 	     const struct nf_hook_state *state,
// 	     struct xt_table *table)
SEC("kprobe/ipt_do_table")
int BPF_KPROBE(ipt_do_table, void *priv, struct sk_buff *skb, struct nf_hook_state *state)
{
	u32 hook;
	bpf_probe_read(&hook, sizeof(hook), &state->hook);
	ipt_do_table_entry(skb, state, priv, hook);
	return 0;
}

SEC("kprobe/ipt_do_table")
int BPF_KPROBE(ipt_do_table310, struct sk_buff *skb, u32 hook, struct nf_hook_state *state)
{
	ipt_do_table_entry(skb, state, PT_REGS_PARM4(ctx), hook);
	return 0;
}

SEC("kretprobe/ipt_do_table")
int BPF_KRETPROBE(ipt_do_table_ret, int ret)
{
	struct sock *sk;
	struct tid_map_value *value;
	struct event event = {};
	u32 tid = bpf_get_current_pid_tgid();
	u64 addr;

	if (ret == NF_DROP)
	{
		value = bpf_map_lookup_elem(&tid_map, &tid);
		if (value == NULL)
			return 0;

		struct nf_hook_state *state = value->state;
		struct xt_table *table = value->table;
		struct sk_buff *skb = value->skb;

		event.type = IPTABLES;
		addr = bpf_core_xt_table_name(table);
		if (addr)
			bpf_probe_read(event.ip.name, sizeof(event.ip.name), (void *)addr);

		event.ip.hook = value->hook;
		bpf_probe_read(&sk, sizeof(sk), &skb->sk);
		fill_stack(ctx, &event);
		fill_pid(&event);
		fill_sk_skb(&event, sk, skb);
		bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU, &event, sizeof(event));
	}
	bpf_map_delete_elem(&tid_map, &tid);
	return 0;
}

SEC("kprobe/__nf_conntrack_confirm")
int BPF_KPROBE(__nf_conntrack_confirm, struct sk_buff *skb)
{
	ipt_do_table_entry(skb, NULL, NULL, 0);
	return 0;
}

SEC("kretprobe/__nf_conntrack_confirm")
int BPF_KRETPROBE(__nf_conntrack_confirm_ret, int ret)
{
	struct event event = {};
	struct tid_map_value *value;
	struct sk_buff *skb;
	struct sock *sk;
	u32 tid = bpf_get_current_pid_tgid();
	if (ret == NF_DROP)
	{
		value = bpf_map_lookup_elem(&tid_map, &tid);
		if (value == NULL)
			return 0;

		event.type = KFREE_SKB;
		skb = value->skb;
		bpf_probe_read(&sk, sizeof(sk), &skb->sk);
		fill_stack(ctx, &event);
		fill_pid(&event);
		fill_sk_skb(&event, sk, skb);
		bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU, &event, sizeof(event));
	}

	bpf_map_delete_elem(&tid_map, &tid);
	return 0;
}

char _license[] SEC("license") = "GPL";