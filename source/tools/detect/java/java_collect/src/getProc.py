# -*- coding: utf-8 -*-
"""
-------------------------------------------------
   File Name：     getProc
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
import time


def calcTotal(cpu_times):
    return cpu_times.user + cpu_times.system + cpu_times.iowait


def getTops(pids):
    dTop = {}
    for pid in pids:
        p = psutil.Process(pid)
        if p:
            dTop[pid] = calcTotal(p.cpu_times())
    return dTop


def diffTop(t1, t2):
    res = {}
    for k, v in t2.items():
        if k in t1:
            res[k] = v - t1[k]
    return res


pids = psutil.pids()
t1 = getTops(pids)
time.sleep(1)
t2 = getTops(pids)
res = diffTop(t1, t2)
print(sorted(res.items(), key=lambda x: x[1], reverse=True))

# for pid in pids:
#     p = psutil.Process(pid)
#     cmds = p.cmdline()
#     if len(cmds) > 0 and 'java' in cmds[0]:
#         print(cmds, calcTotal(p.cpu_times()))


if __name__ == "__main__":
    pass
