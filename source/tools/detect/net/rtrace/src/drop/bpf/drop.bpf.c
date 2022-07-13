#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_endian.h>

#include "drop.h"

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
    __type(value, struct sk_buff *);
} tid_map SEC(".maps");

struct
{
	__uint(type, BPF_MAP_TYPE_STACK_TRACE);
	__uint(max_entries, 1024);
	__uint(key_size, sizeof(u32));
	__uint(value_size, sizeof(uint64_t) * 20);
} stackmap SEC(".maps");

#define NF_DROP 0
#define NF_ACCEPT 1


// __always_inline void fill_event(struct event *event, struct sk_buff *skb)
// {

// }

// SEC("kprobe/__nf_conntrack_confirm")
// int BPF_KPROBE(kprobe____nf_conntrack_confirm, struct sk_buff *skb)
// {
// 	u32 tid = bpf_get_current_pid_tgid();
// 	bpf_map_update_elem(&tid_map, &tid, &skb, BPF_ANY);
// 	return 0;
// }

// SEC("kretprobe/__nf_conntrack_confirm")
// int BPF_KRETPROBE(kretprobe____nf_conntrack_confirm, int ret)
// {
// 	struct sk_buff **skbp;
// 	u32 tid = bpf_get_current_pid_tgid();
// 	if (ret == 0)
// 	{
// 		skbp = bpf_map_lookup_elem(&tid_map, &tid);
// 		if (skbp == NULL)
// 			return 0;
		

// 	}

// 	bpf_map_delete_elem(&tid_map, &tid);
// 	return 0;
// }

SEC("kprobe/kfree_skb")
int BPF_KPROBE(kfree_skb, struct sk_buff *skb)
{
	struct event event = {};
	struct sock *sk = NULL;
	struct net *net = NULL;
	struct iphdr ih = {};
	struct tcphdr th = {};
	struct udphdr uh = {};
	u16 protocol = 0;
	bool has_netheader = false;
	u16 network_header, transport_header;
	char *head;

	event.stackid = bpf_get_stackid(ctx, &stackmap, 0);

	bpf_probe_read(&sk, sizeof(sk), &skb->sk);

	if (sk)
	{
		// address pair
		bpf_probe_read(&event.skap.daddr, sizeof(event.skap.daddr), &sk->__sk_common.skc_daddr);
		bpf_probe_read(&event.skap.dport, sizeof(event.skap.dport), &sk->__sk_common.skc_dport);
		bpf_probe_read(&event.skap.saddr, sizeof(event.skap.saddr), &sk->__sk_common.skc_rcv_saddr);
		bpf_probe_read(&event.skap.sport, sizeof(event.skap.sport), &sk->__sk_common.skc_num);
		event.skap.dport = bpf_ntohs(event.skap.dport);

		bpf_probe_read(&event.sk_protocol, sizeof(event.sk_protocol), &sk->sk_protocol);
		bpf_probe_read(&event.state, sizeof(event.state), &sk->__sk_common.skc_state);
		protocol = event.sk_protocol;
	}

	// pid info
	event.pi.pid = bpf_get_current_pid_tgid() >> 32;
	bpf_get_current_comm(event.pi.comm, sizeof(event.pi.comm));

	bpf_probe_read(&head, sizeof(head), &skb->head);
	bpf_probe_read(&network_header, sizeof(network_header), &skb->network_header);
	if (network_header != 0)
	{
		bpf_probe_read(&ih, sizeof(ih), head + network_header);
		has_netheader = true;
		event.ap.saddr = ih.saddr;
		event.ap.daddr = ih.daddr;
		event.protocol = ih.protocol;
		protocol = protocol == 0 ? event.protocol : protocol;
		transport_header = network_header + (ih.ihl << 2);
	}
	else
	{
		bpf_probe_read(&transport_header, sizeof(transport_header), &skb->transport_header);
	}

	if (protocol == 0)
	{
		event.error = ERR_PROTOCOL_NOT_DETERMINED;
		goto out;
	}

	switch (protocol)
	{
	case IPPROTO_ICMP:
		break;
	case IPPROTO_UDP:
		if (transport_header != 0 && transport_header != 0xffff)
		{
			bpf_probe_read(&uh, sizeof(uh), head + transport_header);
			event.ap.sport = bpf_ntohl(uh.source);
			event.ap.dport = bpf_ntohl(uh.dest);
		}
		break;
	case IPPROTO_TCP:
		if (transport_header != 0 && transport_header != 0xffff)
		{
			bpf_probe_read(&th, sizeof(th), head + transport_header);
			event.ap.sport = bpf_ntohl(th.source);
			event.ap.dport = bpf_ntohl(th.dest);
		}
		break;
	default:
		event.error = ERR_PROTOCOL_NOT_SUPPORT;
		break;
	}

out:
	bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU, &event, sizeof(event));
	return 0;
}

char _license[] SEC("license") = "GPL";