//
// Created by 廖肇燕 on 2023/4/29.
//

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netinet/in.h>

#define ICMP_ECHOREPLY 0
#define ICMP_ECHO

#define BUFSIZE 1500
#define DEFAULT_LEN 56

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

struct icmphdr {
    u8 type;
    u8 code;
    u16 checksum;
    union{
        struct{
            u16 id;
            u16 sequence;
        }echo;
        u32 gateway;
        struct{
            u16 unsed;
            u16 mtu;
        }frag;
    }un;
    u8 data[0];
#define icmp_id un.echo.id
#define icmp_seq un.echo.sequence
};
#define ICMP_HSIZE sizeof(struct icmphdr)

struct iphdr {
    u8 hlen:4, ver:4;
    u8 tos;
    u16 tot_len;
    u16 id;
    u16 frag_off;
    u8 ttl;
    u8 protocol;
    u16 check;
    u32 saddr;
    u32 daddr;
};

#define IP_HSIZE sizeof(struct iphdr)
#define IPVERSION  4

char *hostname;
int datalen = DEFAULT_LEN;
char sendbuf[BUFSIZE];
char recvbuf[BUFSIZE];
int nsent;
int nrecv;
pid_t pid;
struct timeval recvtime;
int sockfd;
struct sockaddr_in dest;
struct sockaddr_in from;
struct sigaction act_alarm;
struct sigaction act_int;

void alarm_handler(int);
void int_handler(int);
void set_sighandler();
void send_ping();
void recv_reply();
u16 checksum(u8 *buf, int len);
int handle_pkt();
void bail(const char *);


struct itimerval val_alarm = {
        .it_interval.tv_sec = 1,
        .it_interval.tv_usec = 0,
        .it_value.tv_sec = 0,
        .it_value.tv_usec = 1
};


