//
// Created by 廖肇燕 on 2023/5/4.
//

#include <pcap.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

int main()
{
    char errBuf[PCAP_ERRBUF_SIZE], * devStr;

    devStr = pcap_lookupdev(errBuf);
    if (devStr)
        printf("success: device: %s\n", devStr);
    else
    {
        printf("error: %s\n", errBuf);
        exit(1);
    }

    /* open a device, wait until a packet arrives */
    pcap_t * device = pcap_open_live(devStr, 65536, 1, 1000, errBuf);
    if (!device)
    {
        printf("error: pcap_open_live(): %s\n", errBuf);
        exit(1);
    }

    /* wait a packet to arrive */
    struct pcap_pkthdr packet;
    const u_char * pktStr = pcap_next(device, &packet);

    if (!pktStr)
    {
        printf("did not capture a packet!\n");
        exit(1);
    }

    printf("Packet len:%d, Bytes:%d, Received time:%s\n", packet.len,
           packet.caplen, ctime((const time_t *)&packet.ts.tv_sec));

    for(int i=0; i < packet.len; ++i)
    {
        printf(" %02x", pktStr[i]);
        if ((i + 1) % 16 == 0)
        {
            printf("\n");
        }
    }
    printf("\n");

    pcap_close(device);
    return 0;
}
