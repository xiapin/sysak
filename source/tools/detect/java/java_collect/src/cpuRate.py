# -*- coding: utf-8 -*-
"""
-------------------------------------------------
   File Name：     cpuRate
   Description :
   Author :       liaozhaoyan
   date：          2023/6/8
-------------------------------------------------
   Change Activity:
                   2023/6/8:
-------------------------------------------------
"""
__author__ = 'liaozhaoyan'

import psutil

for proc in psutil.process_iter(['pid', 'name', 'cpu_percent']):
    try:
        pinfo = proc.as_dict(attrs=['pid', 'name', 'cpu_percent'])
    except psutil.NoSuchProcess:
        pass
    else:
        print(pinfo)

if __name__ == "__main__":
    pass
