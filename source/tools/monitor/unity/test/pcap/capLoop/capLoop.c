//
// Created by 廖肇燕 on 2023/5/4.
//

#include <pcap.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>


void processPacket(u_char *arg, const struct pcap_pkthdr *pkthdr, const u_char *packet)
{
    int *count = (int *)arg;

    printf("Packet Count: %d\n", ++(*count));
    printf("Received Packet Size: %d\n", pkthdr->len);
    printf("Payload:\n");

    for(int i=0; i < pkthdr->len; ++i)
    {
        printf("%02x ", packet[i]);
        if ((i + 1) % 16 == 0)
        {
            printf("\n");
        }
    }
    printf("\n\n");
    return;
}

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
    pcap_t * device = pcap_open_live(devStr, 64, 1, 0, errBuf);
    if (!device)
    {
        printf("error: pcap_open_live(): %s\n", errBuf);
        exit(1);
    }

    int count = 0;
    /*Loop forever & call processPacket() for every received packet.*/
    pcap_loop(device, 30, processPacket, (u_char *)&count);

    pcap_close(device);
    return 0;
}
