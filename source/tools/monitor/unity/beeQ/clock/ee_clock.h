//
// 高效时钟获取方案
// Created by 廖肇燕 on 2023/3/17.
//

#ifndef UNITY_EE_CLOCK_H
#define UNITY_EE_CLOCK_H

typedef unsigned long ee_clock_t;

int calibrate_local_clock();
ee_clock_t get_local_clock();

#endif //UNITY_EE_CLOCK_H
