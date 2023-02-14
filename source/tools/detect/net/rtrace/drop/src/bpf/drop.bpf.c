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


BPF_ARRAY(filter_map, struct drop_filter, 1);
BPF_HASH(inner_tid_map, u32, struct tid_map_value, 1024);
BPF_STACK_TRACE(stackmap, 1024);

#define NF_DROP 0
#define NF_ACCEPT 1

__always_inline void fill_stack(void *ctx, struct drop_event *event)
{
	event->stackid = bpf_get_stackid(ctx, &stackmap, 0);
}

__always_inline void fill_pid(struct drop_event *event)
{
	// pid info
	event->pid = pid();
	comm(event->comm);
}

__always_inline int fill_sk_skb(struct drop_event *event, struct sock *sk, struct sk_buff *skb)
{
	struct net *net = NULL;
	struct iphdr ih = {};
	struct tcphdr th = {};
	struct udphdr uh = {};
	u16 protocol = 0;
	bool has_netheader = false;
	u16 network_header, transport_header;
	char *head;
	event->has_sk = false;
	if (sk)
	{
		event->has_sk = true;
		// address pair
		bpf_probe_read(&event->skap.daddr, sizeof(event->skap.daddr), &sk->__sk_common.skc_daddr);
		bpf_probe_read(&event->skap.dport, sizeof(event->skap.dport), &sk->__sk_common.skc_dport);
		bpf_probe_read(&event->skap.saddr, sizeof(event->skap.saddr), &sk->__sk_common.skc_rcv_saddr);
		bpf_probe_read(&event->skap.sport, sizeof(event->skap.sport), &sk->__sk_common.skc_num);
		event->skap.dport = bpf_ntohs(event->skap.dport);

		protocol = bpf_core_sock_sk_protocol(sk);
		event->sk_protocol = protocol;
		bpf_probe_read(&event->sk_state, sizeof(event->sk_state), &sk->__sk_common.skc_state);
	}

	bpf_probe_read(&head, sizeof(head), &skb->head);
	bpf_probe_read(&network_header, sizeof(network_header), &skb->network_header);
	if (network_header != 0)
	{
		bpf_probe_read(&ih, sizeof(ih), head + network_header);
		has_netheader = true;
		event->skbap.saddr = ih.saddr;
		event->skbap.daddr = ih.daddr;
		event->skb_protocol = ih.protocol;
		
		protocol = ih.protocol;
		transport_header = network_header + (ih.ihl << 2);
	}
	else
	{
		bpf_probe_read(&transport_header, sizeof(transport_header), &skb->transport_header);
	}
	switch (protocol)
	{
	case IPPROTO_ICMP:
		break;
	case IPPROTO_UDP:
		if (transport_header != 0 && transport_header != 0xffff)
		{
			bpf_probe_read(&uh, sizeof(uh), head + transport_header);
			event->skbap.sport = bpf_ntohs(uh.source);
			event->skbap.dport = bpf_ntohs(uh.dest);
		}
		break;
	case IPPROTO_TCP:
		bpf_probe_read(&th, sizeof(th), head + transport_header);
		event->skbap.sport = bpf_ntohs(th.source);
		event->skbap.dport = bpf_ntohs(th.dest);
		break;
	default:
		return -1;
		break;
	}
	return 0;
}

__always_inline void handle(void *ctx, struct sock *sk, struct sk_buff *skb, u32 type)
{
	u32 key = 0;
	struct drop_filter *filter = NULL;
	struct drop_event event = {};

	event.type = type;
	if (fill_sk_skb(&event, sk, skb) < 0)
		return;
	filter = bpf_map_lookup_elem(&filter_map, &key);
	if (filter)
	{
		if (!event.sk_protocol && !event.skb_protocol)
			return;

		if (filter->protocol)
		{
			if (filter->protocol != event.skb_protocol && filter->protocol != event.sk_protocol)
				return;
		}
		
		if (sk)
		{
			// skip Close state
			if (event.sk_state == 7)
				return;
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
		if (filter->ap.daddr && event.skbap.saddr != filter->ap.daddr)
			return;
		if (filter->ap.saddr && event.skbap.daddr != filter->ap.saddr)
			return;
		if (filter->ap.dport && event.skbap.sport != filter->ap.dport)
			return;
		if (filter->ap.sport && event.skbap.dport != filter->ap.sport)
			return;
		
	}

	fill_stack(ctx, &event);
	// pid info
	fill_pid(&event);

	bpf_perf_event_output(ctx, &perf_map, BPF_F_CURRENT_CPU, &event, sizeof(event));
}

SEC("kprobe/tcp_drop")
int BPF_KPROBE(tcp_drop, struct sock *sk, struct sk_buff *skb)
{
	handle(ctx, sk, skb, TCP_DROP);
	return 0;
}

struct kfree_skb_tp_args
{
    u32 pad[2];
    struct sk_buff *skbaddr;
	u64 location;
	u16 protocol;
};

SEC("tracepoint/skb/kfree_skb")
int tp_kfree_skb(struct kfree_skb_tp_args *ctx) 
{
	u32 key = 0;
	struct drop_filter *filter = NULL;
	struct drop_event event = {};

	event.type = TP_KFREE_SKB;
	filter = bpf_map_lookup_elem(&filter_map, &key);
	if (filter) {
		if ( filter->protocol !=0 && filter->protocol != ctx->protocol )
			return 0;
	}

	event.skb_protocol = ctx->protocol;
	event.location = ctx->location;

	return 0;
}

SEC("kprobe/kfree_skb")
int BPF_KPROBE(kfree_skb, struct sk_buff *skb)
{
	struct sock *sk;
	bpf_probe_read(&sk, sizeof(sk), &skb->sk);
	handle(ctx, sk, skb, KFREE_SKB);
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
	bpf_map_update_elem(&inner_tid_map, &tid, &value, BPF_ANY);
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
	struct drop_event event = {};
	u32 tid = bpf_get_current_pid_tgid();
	u64 addr;

	if (ret == NF_DROP)
	{
		value = bpf_map_lookup_elem(&inner_tid_map, &tid);
		if (value == NULL)
			return 0;

		struct nf_hook_state *state = value->state;
		struct xt_table *table = value->table;
		struct sk_buff *skb = value->skb;

		event.type = DROP_IPTABLES_DROP;
		addr = bpf_core_xt_table_name(table);
		if (addr)
			bpf_probe_read(event.name, sizeof(event.name), (void *)addr);

		event.hook = value->hook;
		bpf_probe_read(&sk, sizeof(sk), &skb->sk);
		fill_stack(ctx, &event);
		fill_pid(&event);
		fill_sk_skb(&event, sk, skb);
		bpf_perf_event_output(ctx, &perf_map, BPF_F_CURRENT_CPU, &event, sizeof(event));
	}
	bpf_map_delete_elem(&inner_tid_map, &tid);
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
	struct drop_event event = {};
	struct tid_map_value *value;
	struct sk_buff *skb;
	struct sock *sk;
	u32 tid = bpf_get_current_pid_tgid();
	if (ret == NF_DROP)
	{
		value = bpf_map_lookup_elem(&inner_tid_map, &tid);
		if (value == NULL)
			return 0;

		event.type = NF_CONNTRACK;
		skb = value->skb;
		bpf_probe_read(&sk, sizeof(sk), &skb->sk);
		fill_stack(ctx, &event);
		fill_pid(&event);
		fill_sk_skb(&event, sk, skb);
		bpf_perf_event_output(ctx, &perf_map, BPF_F_CURRENT_CPU, &event, sizeof(event));
	}

	bpf_map_delete_elem(&inner_tid_map, &tid);
	return 0;
}

char _license[] SEC("license") = "GPL";