#ifndef NTOPO_H
#define NTOPO_H

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
        unsigned long long ingress_min;
        unsigned long long ingress_max;
        unsigned long long egress_min;
        unsigned long long egress_max;
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
        unsigned long long in_bytes;
        unsigned long long out_bytes;
        
        unsigned int pid;
        unsigned int client_addr;
        unsigned int server_addr;
        unsigned short sport;
        unsigned short dport;
        unsigned int client_max_rt_us;
        unsigned int client_min_rt_us;
        unsigned int client_tot_rt_us;
        unsigned int client_tot_rt_hz;

        unsigned int server_max_rt_us;
        unsigned int server_min_rt_us;
        unsigned int server_tot_rt_us;
        unsigned int server_tot_rt_hz;

        unsigned int requests;
};

struct pid_info
{
        unsigned int valid;
        unsigned char comm[16];
        unsigned char container_id[128];
};

struct config
{
        int latency_threshold; // ms
};

#endif