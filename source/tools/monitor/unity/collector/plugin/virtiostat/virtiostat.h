//
// Created by 廖肇燕 on 2023/5/7.
//

#ifndef UNITY_VIRTIOSTAT_H
#define UNITY_VIRTIOSTAT_H

#define CMPMAX 16

typedef struct virtio_stat {
    char driver[CMPMAX];
    char dev[12];
    char vqname[12];
    unsigned int in_sgs;
    unsigned int out_sgs;
} virtio_stat_t;

#endif //UNITY_VIRTIOSTAT_H
