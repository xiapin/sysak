# -*- coding: utf-8 -*-
"""
-------------------------------------------------
   File Name：     pingEdge
   Description :
   Author :       liaozhaoyan
   date：          2023/9/20
-------------------------------------------------
   Change Activity:
                   2023/9/20:
-------------------------------------------------
"""
__author__ = 'liaozhaoyan'

import os
import re
import sys
import json
import netifaces
from multiprocessing import Process
from nsenter import Namespace
from icmplib import multiping

OUT_PATH = "/var/sysak/pingEdge"

def pings(ip, ts, interval=0.1):
    gw = netifaces.gateways()['default'][netifaces.AF_INET][0]
    rets = multiping([gw, ip], count=int(ts/interval), interval=interval, timeout=1)
    ret = {}
    for r in rets:
        ret[r.address] = {
            "max": r.max_rtt,
            "avg": r.avg_rtt,
            "send": r.packets_sent,
            "loss": r.packet_loss,
        }
    return ret

class CpingProc(Process):
    def __init__(self, dip, tpid=0, ts=5*60):
        super(CpingProc, self).__init__()
        self._dip = dip
        self._ts = ts
        self._tpid = tpid

    def save(self, res):
        fName = os.path.join(OUT_PATH, "%d.json" % self._tpid)
        with open(fName, "w") as f:
            json.dump(res, f)

    def run(self):
        if self._tpid > 0:
            with Namespace(self._tpid, 'net'):
                res = pings(self._dip, self._ts)
        else:
            res = pings(self._dip, self._ts)
        self.save(res)

def work(dip, pid, ts=5*60):
    ps = []
    ps.append(CpingProc(dip, 0, int(ts/2)))
    if pid > 0:
        ps.append(CpingProc(dip, pid, int(ts/2)))
    for p in ps:
        p.start()
    for p in ps:
        p.join()

def pre(distDir):
    if not os.path.exists(distDir):
        os.makedirs(distDir)
    fNames = os.listdir(distDir)
    for fName in fNames:
        if fName.endswith(".json"):
            os.remove(os.path.join(distDir, fName))

def post(distDir):
    res = {}
    fNames = os.listdir(distDir)
    for fName in fNames:
        if fName.endswith(".json"):
            with open(os.path.join(distDir, fName), 'r') as f:
                res[fName] = json.load(f)
    print(res)


if __name__ == "__main__":
    pre(OUT_PATH)
    dip = sys.argv[1]
    if dip == "-h" or dip == "--help":
        print("pingEdge [dist IP] [pid of container process] [minutes], minute is optional ,default is 5.")
        sys.exit(0)
    pid = 0
    if len(sys.argv) > 2:
        pid = int(sys.argv[2])
    ts = 5 * 60
    if len(sys.argv) > 3:
        ts = int(sys.argv[3]) * 60
    work(dip, pid, ts)
    post(OUT_PATH)
    pass
