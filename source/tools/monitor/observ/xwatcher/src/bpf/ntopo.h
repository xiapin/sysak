#ifndef XWATCHER_H
#define XWATCHER_H

enum role
{
    ROLE_UNKNOWN,
    ROLE_CLIENT,
    ROLE_SERVER,
};

struct addrpair
{
    unsigned int saddr;
    unsigned int daddr;
    unsigned short sport;
    unsigned short dport;
};

struct sock_info
{
    struct addrpair ap;
    enum role role;

    unsigned int pid;
    unsigned long long in_bytes;
    unsigned long long out_bytes;
};

struct edge_info_key
{
    unsigned int saddr;
    unsigned int daddr;
};

struct edge_info
{
    int empty;
};

struct node_info_key
{
    unsigned int addr;
};

struct node_info
{
    int empty;
};

struct pid_info
{
    unsigned long long in_bytes;
    unsigned long long out_bytes;
};

#endif