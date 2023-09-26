# -*- coding: utf-8 -*-
"""
-------------------------------------------------
   File Name：     ptop
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


class Cptop(object):
    def __init__(self, interval=0.5):
        super(Cptop, self).__init__()
        self._initerval = interval


    @staticmethod
    def _calcTotal(cpu_times):
        return cpu_times.user + cpu_times.system + cpu_times.iowait

    @staticmethod
    def _diffTop(t1, t2):
        res = {}
        for k, v in t2.items():
            if k in t1:
                res[k] = v - t1[k]
        return res

    def _getTops(self, pids):
        dTop = {}
        for pid in pids:
            try:
                p = psutil.Process(pid)
            except psutil.NoSuchProcess:
                continue
            if p:
                dTop[pid] = self._calcTotal(p.cpu_times())
        return dTop

    def _top(self):
        pids = psutil.pids()
        t1 = self._getTops(pids)
        time.sleep(self._initerval)
        t2 = self._getTops(pids)
        res = self._diffTop(t1, t2)
        return sorted(res.items(), key=lambda x: x[1], reverse=True)

    def top(self, N=10):
        arr = self._top()
        return arr[:N]

    def jtop(self, N=3):
        ret = []
        arr = self._top()
        for a in arr:
            try:
                p = psutil.Process(a[0])
            except psutil.NoSuchProcess:
                p = None
            if p:
                cmds = p.exe()
                if cmds.endswith("java"):
                    ret.append(p)
        return ret[:N]


if __name__ == "__main__":
    t = Cptop()
    tops = t.jtop(4)
    for a in tops:
        print('\t', a.exe())
    pass
