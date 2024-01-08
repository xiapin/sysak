#include "vmlinux.h"

#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_endian.h>

unsigned long long load_byte(void *skb,
			     unsigned long long off) asm("llvm.bpf.load.byte");
unsigned long long load_half(void *skb,
			     unsigned long long off) asm("llvm.bpf.load.half");
unsigned long long load_word(void *skb,
			     unsigned long long off) asm("llvm.bpf.load.word");

SEC("socket")
int socket_tcp(struct __sk_buff *skb)
{
    __u64 nhoff = 0;
    __u64 ip_proto;
    __u64 verlen;
    u32 ports;
    u16 dport;

    ip_proto = load_byte(skb, nhoff + offsetof(struct iphdr, protocol));
    if (ip_proto != 6 )
        return 0;
    
    verlen = load_byte(skb, nhoff + 0);
	if (verlen == 0x45)
		nhoff += 20;
	else
		nhoff += (verlen & 0xF) << 2;

    ports = load_word(skb, nhoff);
    dport = (u16)ports;
    if (dport == 40330)
        return -1;
    return 0;
}


char _license[] SEC("license") = "GPL";