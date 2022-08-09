#ifndef __DROP_H
#define __DROP_H

#ifndef u8
typedef unsigned char u8;
#endif

#ifndef u16
typedef unsigned short int u16;
#endif

#ifndef u32
typedef unsigned int u32;
#endif

#ifndef u64
typedef long long unsigned int u64;
#endif

enum{
    ERR_OK = 0,
    ERR_PROTOCOL_NOT_DETERMINED,
    ERR_PROTOCOL_NOT_SUPPORT,
};

enum {
    KFREE_SKB = 0,
    TCP_DROP,
    IPTABLES,
};
/**
 * struct addr_pair -
 * @saddr: be
 * @sport:
 * @daddr: be
 * @dport:
 */
struct addr_pair
{
    u32 saddr;
    u16 sport;
    u16 dport;
    u32 daddr;
};

struct filter
{
    struct addr_pair ap;
};

struct pid_info
{
    u32 pid;
    u8 comm[16];
};

struct tcp_params
{
    u32 syn_qlen; // syn queue
    u32 max_len;
    u8 syncookies;
    u32 acc_qlen; // accept queue
};

struct iptables_params
{
    u8 name[32];
    u32 hook;
};

struct event
{
    int stackid;
    u8 error;
    u8 type;
    struct pid_info pi;

    struct addr_pair ap;
    struct addr_pair skap;

    u8 state; // tcp state

    u16 sk_protocol;
    u16 protocol;

    union
    {
        struct tcp_params tp;
#define IPTABLE_FILTER 1
#define IPTABLE_MANGLE 2
#define IPTABLE_RAW 3
#define ARPTABLE_FILTER 4
#define IPTABLE_SECURITY 5
#define IPTABLE_NAT 6
        struct iptables_params ip;
        
    };
};

#endif
