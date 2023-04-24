#!/usr/bin/python
# -*- coding: utf-8 -*-

import os
import sys
import signal
import string
import time
import re
import json
from collections import OrderedDict
from diskstatClass import diskstatClass
from common import getDevtRegion
from common import humConvert,echoFile
from common import getContainerId


class iostatClass(diskstatClass):
    def __init__(
        self, devname, pid, utilThresh, iopsThresh, bwThresh,
        top, json, nodiskStat, miscStat, Pattern):
        super(iostatClass, self).__init__(
            devname, utilThresh, json, nodiskStat, Pattern)
        self.pid = pid
        self.miscStat = miscStat
        self.top = int(top) if top is not None else 99999999
        self.iopsThresh = int(iopsThresh) if iopsThresh is not None else 0
        self.bwThresh = int(bwThresh) if bwThresh is not None else 0
        self.devt = min(getDevtRegion(devname)) if devname is not None else -1
        self.tracingDir = "/sys/kernel/debug/tracing/instances/iofsstat4io"
        self.blkTraceDir = self.tracingDir+"/events/block"

    def config(self):
        devt = self.devt
        if not os.path.exists(self.tracingDir):
            os.mkdir(self.tracingDir)
        if devt > 0:
            echoFile(self.blkTraceDir+"/block_getrq/filter", "dev=="+str(devt))
        else:
            echoFile(self.blkTraceDir+"/block_getrq/filter", "")
        echoFile(self.blkTraceDir+"/block_getrq/enable", "1")

    def start(self):
        echoFile(self.tracingDir+"/trace", "")
        echoFile(self.tracingDir+"/tracing_on", "1")
        super(iostatClass, self).start()

    def stop(self):
        echoFile(self.tracingDir+"/tracing_on", "0")
        super(iostatClass, self).stop()

    def clear(self):
        echoFile(self.blkTraceDir+"/block_getrq/enable", "0")
        if self.devt > 0:
            echoFile(self.blkTraceDir+"/block_getrq/filter", "0")
        super(iostatClass, self).clear()

    def showJson(self, stat):
        secs = self.cycle
        statJsonStr = '{"time":"","iostats":[]}'
        iostatDicts = json.loads(statJsonStr, object_pairs_hook=OrderedDict)
        iostatDicts['time'] = time.strftime(
            '%Y/%m/%d %H:%M:%S', time.localtime())
        stSecs = str(secs)+'s' if secs > 1 else 's'
        for key, item in stat.items():
            if (item["iops_rd"] + item["iops_wr"]) == 0:
                continue
            item["bps_rd"] = \
                humConvert(item["bps_rd"], True).replace('s', stSecs) if item["bps_rd"] else 0
            item["bps_wr"] = \
                humConvert(item["bps_wr"], True).replace('s', stSecs) if item["bps_wr"] else 0
            iostatJsonStr = '{\
                "comm":"","pid":0,"bps_rd":0,"iops_rd":0,"iops_wr":0,"bps_wr":0,"device":0}'
            iostatDict = json.loads(iostatJsonStr, object_pairs_hook=OrderedDict)
            for key in ['comm', 'pid', 'bps_rd', 'iops_rd', 'iops_wr', 'bps_wr', 'device']:
                iostatDict[key] = item[key]
            iostatDicts["iostats"].append(iostatDict)
        if len(iostatDicts["iostats"]) > 0:
            self.writeDataToJson(json.dumps(iostatDicts))

    def patternIdx(self, size):
        dp = [
            ("pat_W4K", (4*1024)), ("pat_W16K", (16*1024)),
            ("pat_W32K", (32*1024)), ("pat_W64K",(64*1024)),
            ("pat_W128K", (128*1024)), ("pat_W256K", (256*1024)),
            ("pat_W512K", (512*1024))]
        for d in dp:
            if size <= d[1]:
                return d[0]
        return 'pat_Wlarge'

    def patternPercent(self, pat, total):
        if total == 0 or pat == 0:
            return '0'
        return format(pat / (total * 1.0) * 100, '.2f') + '%'

    def show(self):
        iopsTotal = 0
        WrIopsTotal = 0
        RdIopsTotal = 0
        bwTotal = 0
        WrBwTotal = 0
        RdBwTotal = 0
        stat = {}
        mStat = {}
        secs = self.cycle
        with open(self.tracingDir+"/trace") as f:
            traceText = list(
                filter(lambda x: 'block_getrq' in x, f.readlines()))
        # jbd2/vda1-8-358 ... : block_getrq: 253,0 WS 59098136 + 120 [jbd2/vda1-8]
        for entry in traceText:
            oneIO = entry.split()
            matchObj = re.match(
                r'(.*) \[([^\[\]]*)\] (.*) \[([^\[\]]*)\]\n', entry)
            comm = matchObj.group(4)
            pid = matchObj.group(1).rsplit('-', 1)[1].strip()
            if self.pid is not None and int(pid) != self.pid:
                continue
            devinfo = oneIO[-6-comm.count(' ')].split(',')
            dev = ((int(devinfo[0]) << 20) + int(devinfo[1]))
            if str(dev) == '0':
                continue
            device = self.getDevNameByDevt(dev)
            if device == '-' or self.notCareDevice(device) == True:
                continue
            if self.miscStat is not None:
                if not mStat.has_key(device):
                    mStat.setdefault(device, {})
                stat = mStat[device]
            iotype = oneIO[-5-comm.count(' ')]
            sectors = oneIO[-2-comm.count(' ')]
            task = str(pid)+':'+device
            if bool(stat.has_key(task)) != True:
                stat.setdefault(task,
                                {"comm":"", "pid": pid, "iops_rd": 0,
                                "iops_wr": 0, "bps_rd": 0, "bps_wr": 0,
                                "flushIO": 0, "device": device,
                                "cid":getContainerId(pid),
                                "pat_W4K":0, "pat_W16K":0, "pat_W32K":0,
                                "pat_W64K":0, "pat_W128K":0, "pat_W256K":0,
                                "pat_W512K":0, "pat_Wlarge":0})
            size = int(sectors) * 512
            if len(comm) > 0:
                stat[task]["comm"] = comm
            if 'R' in iotype:
                stat[task]["iops_rd"] += 1
                stat[task]["bps_rd"] += size
                bwTotal += size
                iopsTotal += 1
            if 'W' in iotype:
                stat[task]["iops_wr"] += 1
                stat[task]["bps_wr"] += size
                bwTotal += size
                iopsTotal += 1
                if self.Pattern and size > 0 and size < 1024 * 1024 * 100:
                    stat[task][self.patternIdx(size)] += 1
            if 'F' in iotype:
                stat[task]["flushIO"] += 1

        if self.iopsThresh or self.bwThresh:
            if (self.bwThresh and bwTotal >= self.bwThresh) or \
                (self.iopsThresh and iopsTotal >= self.iopsThresh):
                pass
            else:
                return

        if self.enableJsonShow() == False:
            print(time.strftime('%Y/%m/%d %H:%M:%S', time.localtime()))
        super(iostatClass, self).show()

        if self.miscStat is not None:
            tmpStat = []
            for d,val in mStat.items():
                s = sorted(val.items(),
                    key=lambda e: (e[1]["bps_wr"]+e[1]["bps_rd"]),
                    reverse=True)[:self.top]
                tmpStat.append((d, s))
            del self.miscStat[:]
            self.miscStat.extend(tmpStat)
            return

        stat = sorted(stat.items(),
                    key=lambda e: (e[1]["iops_rd"] + e[1]["iops_wr"]),
                    reverse=True)[:self.top]

        if self.enableJsonShow() == True:
            self.showJson(stat)
            return

        tPattern = ''
        if self.Pattern:
            WrIopsTotal = 0
            RdIopsTotal = 0
            WrBwTotal = 0
            RdBwTotal = 0
            tPattern = ('%-12s%-12s%-12s%-12s%-12s%-12s%-12s%-12s' % (
                "pat_W4K", "pat_W16K", "pat_W32K", "pat_W64K", "pat_W128K",
                "pat_W256K", "pat_W512K", "pat_Wlarge"
            ))
        print('%-20s%-8s%-24s%-12s%-16s%-12s%-12s%-12s%s' %
              ("comm", "pid", "cid", "iops_rd", "bps_rd", "iops_wr", "bps_wr",
              "device", tPattern))
        stSecs = str(secs)+'s' if secs > 1 else 's'
        for key, item in stat:
            if (item["iops_rd"] + item["iops_wr"]) == 0:
                continue
            patPercent = ''
            if self.Pattern:
                WrIopsTotal += item["iops_wr"]
                RdIopsTotal += item["iops_rd"]
                WrBwTotal += item["bps_wr"]
                RdBwTotal += item["bps_rd"]
                patPercent = ('%-12s%-12s%-12s%-12s%-12s%-12s%-12s%-12s' % (
                   self.patternPercent(item["pat_W4K"], item["iops_wr"]),
                   self.patternPercent(item["pat_W16K"], item["iops_wr"]),
                   self.patternPercent(item["pat_W32K"], item["iops_wr"]),
                   self.patternPercent(item["pat_W64K"], item["iops_wr"]),
                   self.patternPercent(item["pat_W128K"], item["iops_wr"]),
                   self.patternPercent(item["pat_W256K"], item["iops_wr"]),
                   self.patternPercent(item["pat_W512K"], item["iops_wr"]),
                   self.patternPercent(item["pat_Wlarge"], item["iops_wr"])
                ))
            item["bps_rd"] = \
                humConvert(item["bps_rd"], True).replace('s', stSecs) if item["bps_rd"] else 0
            item["bps_wr"] = \
                humConvert(item["bps_wr"], True).replace('s', stSecs) if item["bps_wr"] else 0
            patPercent += item["cid"]
            print('%-20s%-8s%-24s%-12s%-16s%-12s%-12s%-12s%s' % (item["comm"],
                str(item["pid"]), item["cid"][0:20], str(item["iops_rd"]), 
                item["bps_rd"], str(item["iops_wr"]), item["bps_wr"], 
                item["device"], patPercent))
        if self.Pattern:
            print('totalIops:%d(r:%d, w:%d), totalBw:%s(r:%s, w:%s)' %
                (iopsTotal, RdIopsTotal, WrIopsTotal,
                (humConvert(bwTotal, True).replace('s', stSecs) if bwTotal else 0),
                (humConvert(RdBwTotal, True).replace('s', stSecs) if RdBwTotal else 0),
                (humConvert(WrBwTotal, True).replace('s', stSecs) if WrBwTotal else 0)))
        print("")

    def entry(self, interval):
        self.start()
        time.sleep(float(interval))
        self.stop()
        self.show()
