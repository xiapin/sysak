//
// Created by 廖肇燕 on 2023/2/24.
//

#ifndef UNITY_NET_RETRANS_H
#define UNITY_NET_RETRANS_H

#define TASK_COMM_LEN 16

enum {
    NET_RETRANS_TYPE_RTO,
    NET_RETRANS_TYPE_ZERO,
    NET_RETRANS_TYPE_RST,
    NET_RETRANS_TYPE_RST_SK,
    NET_RETRANS_TYPE_RST_ACTIVE,
    NET_RETRANS_TYPE_SYN,
    NET_RETRANS_TYPE_SYN_ACK,
    NET_RETRANS_TYPE_MAX,
};


struct data_t {
    char comm[TASK_COMM_LEN];
    unsigned int pid;
    unsigned int  type;
    unsigned int  ip_src;
    unsigned int  ip_dst;
    unsigned short sport;
    unsigned short dport;
    unsigned short sk_state;
    unsigned short stack_id;

    unsigned int  rcv_nxt;
    unsigned int  rcv_wup;
    unsigned int  snd_nxt;
    unsigned int  snd_una;
    unsigned int  copied_seq;
    unsigned int  snd_wnd;
    unsigned int  rcv_wnd;

    unsigned int  lost_out;
    unsigned int  packets_out;
    unsigned int  retrans_out;
    unsigned int  sacked_out;
    unsigned int  reordering;
};

#endif //UNITY_NET_RETRANS_H
