//
// Created by 廖肇燕 on 2023/5/4.
//

#include <pcap.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

#define u_char unsigned char
#define u_short unsigned short
#define u_int unsigned int
#define uint16_t unsigned short int

/* 以太网帧头部 */
struct sniff_ethernet {
#define ETHER_ADDR_LEN	6
    u_char ether_dhost[ETHER_ADDR_LEN]; /* 目的主机的地址 */
    u_char ether_shost[ETHER_ADDR_LEN]; /* 源主机的地址 */
    u_short ether_type; /* IP：0x0800;IPV6:0x86DD; ARP:0x0806;RARP:0x8035 */
};
#define ETHERTYPE_IPV4  (0x0800)
#define ETHERTYPE_IPV6	(0x86DD)
#define ETHERTYPE_ARP	(0x0806)
#define ETHERTYPE_RARP	(0x8035)


/* IP数据包的头部 */
struct sniff_ip {
#if BYTE_ORDER == LITTLE_ENDIAN
    u_int ip_hl:4, /* 头部长度 */
    ip_v:4; /* 版本号 */
#if BYTE_ORDER == BIG_ENDIAN
    u_int ip_v:4, /* 版本号 */
    ip_hl:4; /* 头部长度 */
#endif
#endif /* not _IP_VHL */
    u_char ip_tos; /* 服务的类型 */
    u_short ip_len; /* 总长度 */
    u_short ip_id; /*包标志号 */
    u_short ip_off; /* 碎片偏移 */
#define IP_RF 0x8000 /* 保留的碎片标志 */
#define IP_DF 0x4000 /* dont fragment flag */
#define IP_MF 0x2000 /* 多碎片标志*/
#define IP_OFFMASK 0x1fff /*分段位 */
    u_char ip_ttl; /* 数据包的生存时间 */
    u_char ip_p; /* 所使用的协议:1 ICMP;2 IGMP;4 IP;6 TCP;17 UDP;89 OSPF */
    u_short ip_sum; /* 校验和 */
    struct in_addr ip_src,ip_dst; /* 源地址、目的地址*/
};
#define IPTYPE_ICMP		(1)
#define IPTYPE_IGMP		(2)
#define IPTYPE_IP		(4)
#define IPTYPE_TCP		(6)
#define IPTYPE_UDP		(17)
#define IPTYPE_OSPF		(89)

typedef u_int tcp_seq;
/* TCP 数据包的头部 */
struct sniff_tcp {
    u_short th_sport; /* 源端口 */
    u_short th_dport; /* 目的端口 */
    tcp_seq th_seq; /* 包序号 */
    tcp_seq th_ack; /* 确认序号 */
#if BYTE_ORDER == LITTLE_ENDIAN
    u_int th_x2:4, /* 还没有用到 */
    th_off:4; /* 数据偏移 */
#endif
#if BYTE_ORDER == BIG_ENDIAN
    u_int th_off:4, /* 数据偏移*/
    th_x2:4; /*还没有用到 */
#endif
    u_char th_flags;
#define TH_FIN 0x01
#define TH_SYN 0x02
#define TH_RST 0x04
#define TH_PUSH 0x08
#define TH_ACK 0x10
#define TH_URG 0x20
#define TH_ECE 0x40
#define TH_CWR 0x80
#define TH_FLAGS (TH_FINTH_SYNTH_RSTTH_ACKTH_URGTH_ECETH_CWR)
    u_short th_win; /* TCP滑动窗口 */
    u_short th_sum; /* 头部校验和 */
    u_short th_urp; /* 紧急服务位 */
};


/* UDP header */
struct sniff_udp{
    uint16_t sport;		/* source port */
    uint16_t dport;		/* destination port */
    uint16_t udp_length;
    uint16_t udp_sum;		/* checksum */
};

int pcap_protocal(const struct pcap_pkthdr *pkthdr, const u_char *packet,pcap_dumper_t* arg)
{
    pcap_dump((char *)arg, pkthdr, packet);
    printf("packet size:%u, data len:%u\n",  pkthdr->len, pkthdr->caplen); //数据包实际的长度, 抓到时的数据长度

    struct sniff_ethernet *ethernet = (struct sniff_ethernet*)packet;
    unsigned char* src_mac = ethernet->ether_shost;
    unsigned char* dst_mac = ethernet->ether_dhost;

    printf("src_mac:%x:%x:%x:%x:%x:%x\n",src_mac[0],src_mac[1],src_mac[2],src_mac[3],src_mac[4],src_mac[5]);
    printf("dst_mac:%x:%x:%x:%x:%x:%x\n",dst_mac[0],dst_mac[1],dst_mac[2],dst_mac[3],dst_mac[4],dst_mac[5]);
    printf("ether_type:%u\n",ethernet->ether_type);

    int eth_len = sizeof(struct sniff_ethernet);  //以太网头的长度
    int ip_len = sizeof(struct sniff_ip); //ip头的长度
    int tcp_len = sizeof(struct sniff_tcp);  //tcp头的长度
    int udp_len = sizeof(struct sniff_udp);  //udp头的长度
    printf("eth_len: %d\n",eth_len);
    printf("ip_len: %d\n",ip_len);
    printf("tcp_len: %d\n",tcp_len);
    printf("udp_len: %d\n",udp_len);
    printf("/************************************/\n");

    /*解析网络层  IP头*/
    //ntohs()是一个函数名，作用是将一个16位数由网络字节顺序转换为主机字节顺序。
    if(ntohs(ethernet->ether_type) == ETHERTYPE_IPV4)
    {  //IPV4
        printf("/**********************************************************************/\n");
        printf("It's IPv4!\n");
        struct sniff_ip* ip = (struct sniff_ip*)(packet + eth_len);
        printf("ip->ip_hl:%d\n",ip->ip_hl);
        printf("ip->ip_hl & 0x0f:%x\n",ip->ip_hl & 0x0f);
        ip_len = (ip->ip_hl & 0x0f)*4;            //ip头的长度
        printf("ip->ip_v:%d\n",ip->ip_v);
        // printf("ip->ip_tos:%s\n",ip->ip_tos);
        printf("ip->ip_len:%d\n",ip->ip_len);
        // struct in_addr
        // {
        // 	in_addr_t s_addr;
        // };
        unsigned char *saddr = (unsigned char*)&ip->ip_src.s_addr; //网络字节序转换成主机字节序
        unsigned char *daddr = (unsigned char*)&ip->ip_dst.s_addr;

        //printf("eth_len:%u  ip_len:%u  tcp_len:%u  udp_len:%u\n", eth_len, ip_len, tcp_len, udp_len);
        printf("src_ip:%d.%d.%d.%d\n", saddr[0], saddr[1],saddr[2],saddr[3]/*InttoIpv4str(saddr)*/);  //源IP地址
        printf("dst_ip:%d.%d.%d.%d\n", daddr[0],daddr[1],daddr[2],daddr[3]/*InttoIpv4str(daddr)*/);  //目的IP地址

        /*解析传输层  TCP、UDP、ICMP*/

        if(ip->ip_p == IPTYPE_TCP)
        {         //TCP
            printf("ip->proto:TCP\n");      //传输层用的哪一个协议
            struct sniff_tcp* tcp = (struct sniff_tcp*)(packet + eth_len + ip_len);
            printf("tcp_sport = %u\n", tcp->th_sport);
            printf("tcp_dport = %u\n", tcp->th_dport);
            for(int i=0;*(packet + eth_len + ip_len+tcp_len+i)!='\0';i++)
            {
                printf("%02x ",*(packet + eth_len + ip_len+tcp_len+i));
            }

            /**********(pcaket + eth_len + ip_len + tcp_len)就是TCP协议传输的正文数据了***********/
        }
        else if(ip->ip_p  == IPTYPE_UDP)
        {  //UDP
            printf("ip->proto:UDP\n");      //传输层用的哪一个协议
            struct sniff_udp* udp = (struct sniff_udp*)(packet + eth_len + ip_len);
            printf("udp_sport = %u\n", udp->sport);
            printf("udp_dport = %u\n", udp->dport);
            /**********(pcaket + eth_len + ip_len + udp_len)就是UDP协议传输的正文数据了***********/
        }
        else if(ip->ip_p == IPTYPE_ICMP)
        {   //ICMP
            printf("ip->proto:CCMP\n");      //传输层用的哪一个协议
        }

    }
    else if(ntohs(ethernet->ether_type) == ETHERTYPE_IPV6)
    { //IPV6
        printf("It's IPv6!\n");
    }
    else{
        printf("既不是IPV4也不是IPV6\n");
    }
    printf("============================================\n");
    return 0;
}

int main()
{
    int ret32 = -1;
    pcap_t *handle = NULL; /* 会话的句柄 */
    char *dev = "lo"; /* 执行嗅探的设备 */
    char errbuf[PCAP_ERRBUF_SIZE]; /* 存储错误 信息的字符串 */
    struct bpf_program filter; /*已经编译好的过滤表达式*/
    char filter_app[] = "port 7890"; /* 过滤表达式*/
    bpf_u_int32 mask; /* 执行嗅探的设备的网络掩码 */
    bpf_u_int32 net; /* 执行嗅探的设备的IP地址 */
    const u_char *packet; /* 实际的包 */
    struct pcap_pkthdr header; /* 由pcap.h定义 */
    struct in_addr ip_addr;
    int pcapnum=0;

    /*开启支持PCAP的设备嗅探*/
    // dev = pcap_lookupdev(errbuf);
    ret32 = pcap_lookupnet(dev, &net, &mask, errbuf);//获取指定设备的网络号与掩码，如果出错，返回-1，errbuf存放错误信息
    printf("Device: %s\n", dev);
    if(ret32 < 0)
    {
        printf("pcap_lookupnet return %d, errbuf:%s\n", ret32, errbuf);
    }
    printf("sizeof(mask) = %d, mask:%#x, net:%#x\n",sizeof(mask), mask, net);
    ip_addr.s_addr= net;
    printf("ipaddress is :%s\n",inet_ntoa(ip_addr));


    handle = pcap_open_live(dev, 10*1024, 1, 0, errbuf);
    // printf("handle = %s\n",handle);
    //捕获网络数据包的数据包捕获描述字，打开名为DEV的网络设备，最大捕获字节为1024字节，
    //将网络接口设定为混杂模式，超时时间为0ms意味着一直嗅探直到捕获到数据
    if(handle == NULL)
    {
        printf("pcap_open_live return err,errbuf:%s...\n", errbuf);
        return -1;
    }

    pcap_dumper_t* out_pcap;
    out_pcap  = pcap_dump_open(handle,"protocal.pcap");

    while(1)
    {
        pcapnum++;
        if(pcapnum>100)
        {
            break;
        }
        /* 截获一个包 */
        packet = pcap_next(handle, &header);
        if(packet)
        {
            /* 打印它的长度 */
            printf("Jacked a packet with length of [%d]\n", header.len);
            //数据包协议解析
            pcap_protocal(&header, packet,out_pcap);
        }
        else
        {
            printf("pcap_next return err, errbuf:%s\n", errbuf);
            break;
        }
    }

    /*flush buff*/
    pcap_dump_flush(out_pcap);

    pcap_dump_close(out_pcap);
    /* 关闭会话 */
    pcap_close(handle);

    return 0;
}

