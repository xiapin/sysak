#ifndef TCPPROBE_H
#define TCPPROBE_H

#define IRQ_RING_SIZE 4
#define SOFTIRQ_RING_SIZE 4
#define WAKEUP_RING_SIZE 4
#define SCHEDSWITCH_RING_SIZE 8

enum TCPPING_STAGE
{
    TCPPING_TX_USR = 0,
    TCPPING_TX_ENTRY,
    TCPPING_TX_EXIT,
    TCPPING_RX_ENTRY,
    TCPPING_RX_EXIT,
    TCPPING_RX_USR,
    TCPPROBE_STAGE_MAX,
};

struct filter
{
    int pid;
    unsigned short be_lport;
    unsigned short be_rport;
    unsigned short lport;
    unsigned short rport;
    unsigned long long sock;
};

struct tcpping_stage
{
    unsigned long long ts;
};

struct irq
{
    unsigned long long tss[IRQ_RING_SIZE];
    unsigned long long cnt;
};

struct softirq
{
    unsigned long long tss[SOFTIRQ_RING_SIZE];
    unsigned long long cnt;
};

struct wakeup
{
    unsigned long long tss[SOFTIRQ_RING_SIZE];
    unsigned long long cnt;
};

struct sched
{
    struct
    {
        int prev_pid;
        int next_pid;
        unsigned char prev_comm[16];
        unsigned char next_comm[16];
        unsigned long long ts;
    } ss[SCHEDSWITCH_RING_SIZE];
    unsigned long long cnt;
};

struct tcpping
{
    struct tcpping_stage stages[TCPPROBE_STAGE_MAX];
    struct irq irq;
    struct softirq sirq;
    struct wakeup wu;
    struct sched sched;
};

#endif
