//
// Created by 廖肇燕 on 2023/5/4.
//

#include <pcap.h>
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

    return 0;
}
