//
// Created by 廖肇燕 on 2023/5/5.
//

#include <pcap.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

static int pack_cnt = 0;

void processPacket(u_char *arg, const struct pcap_pkthdr *pkthdr, const u_char *packet)
{
    pcap_dump(arg, pkthdr, packet);
    pack_cnt ++;
    return;
}

int main(int argc, char * argv[]) {
    int ret;
    time_t hope;
    char errBuf[PCAP_ERRBUF_SIZE], *devStr;

    if (argc < 2) {
        printf("argc %d is less than 2\n", argc);
        exit(1);
    }

    devStr = pcap_lookupdev(errBuf);
    if (devStr)
        printf("success: device: %s\n", devStr);
    else
    {
        printf("error: %s\n", errBuf);
        exit(1);
    }

    /* open a device, wait until a packet arrives */
    pcap_t * device = pcap_open_live(NULL, 96, 1, 1000, errBuf);
    if (!device)
    {
        fprintf(stderr, "error: pcap_open_live(): %s\n", errBuf);
        exit(1);
    }

    struct bpf_program filter;
//    ret = pcap_compile(device, &filter, "tcp and not port 22", 1, 0);
    printf("filter: %s\n", argv[1]);
    ret = pcap_compile(device, &filter, argv[1], 1, 0);
    if (ret < 0) {
        fprintf(stderr, "pcap_compile: %s\n", pcap_geterr(device));
        exit(1);
    }
    ret = pcap_setfilter(device, &filter);
    if (ret < 0) {
        fprintf(stderr, "pcap_setfilter: %s\n", pcap_geterr(device));
        exit(1);
    }

    /*open pcap write output file*/
    pcap_dumper_t* out_pcap;
    out_pcap  = pcap_dump_open(device,"pack.pcap");

    hope = 30 + time(NULL);
    while (time(NULL) <= hope) {
        int ret = pcap_dispatch(device, -1, processPacket, (u_char *)out_pcap);
        if (ret == -1) {
            fprintf(stderr, "pcap_dispatch: %s\n", pcap_geterr(device));
            break;
        }

        if (ret == -2) {
            printf("pcap_breakloop called\n");
            break;
        }
    }
    printf("cap %d\n", pack_cnt);

    /*flush buff*/
    pcap_dump_flush(out_pcap);

    pcap_dump_close(out_pcap);
    pcap_close(device);
    return 0;
}
