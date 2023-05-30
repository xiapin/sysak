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
from common import humConvert
from iostatClass import iostatClass
from fsstatClass import fsstatClass


class promiscClass():
    def __init__(
        self, devname, utilThresh, iopsThresh, bwThresh, top, json,
        nodiskStat, Pattern):
        self._iostat = []
        self._fsstat = []
        self.fs = fsstatClass(devname, None, utilThresh, bwThresh,
                              top, json, nodiskStat, self._fsstat, Pattern)
        self.io = iostatClass(devname, None, utilThresh, iopsThresh,
                              bwThresh, top, json, nodiskStat, self._iostat, 
                              Pattern)


    def _selectKworker(self, iostat, fsItem, kworker):
        select = None
        largeFound = False
        diff = sys.maxsize
        for k in kworker:
            fsRestBw = fsItem["bw_wr"]
            if 'restBW' in fsItem.keys():
                fsRestBw = fsItem["restBW"]
            if fsRestBw > (iostat[k]["bps_wr"] * 15) or \
                (fsRestBw * 50) < iostat[k]["bps_wr"]:
                continue
            d = abs(fsItem["bw_wr"] - iostat[k]["bps_wr"])
            diff = min(d, diff)
            if iostat[k]["bps_wr"] > fsItem["bw_wr"]:
                if not largeFound or diff == d:
                    select = k
                largeFound = True
                continue
            if not largeFound and diff == d:
                    select = k
        return select


    def _addBioToKworker(self, iostat, kworker, fsItem):
        repeated = False
        k = self._selectKworker(iostat, fsItem, kworker)
        if not k:
            return False, 0
        if 'bufferio' not in iostat[k].keys():
            iostat[k].setdefault('bufferio', [])
        task = fsItem["comm"]+':'+str(fsItem["tgid"])+':'+str(fsItem["pid"])+\
            ':'+fsItem["cid"][0:20]
        bio = {'task': task, 'Wrbw': fsItem["bw_wr"], 'file': fsItem["file"],
            'device': fsItem["device"]}
        for d in iostat[k]["bufferio"]:
            if task == d['task'] and d['file'] == bio['file'] and \
                d['device'] == bio['device']:
                d['Wrbw'] = max(d['Wrbw'], bio["Wrbw"])
                repeated = True
                break
        if not repeated:
            iostat[k]["bufferio"].append(bio)
        return True, iostat[k]["bps_wr"]


    def _checkDeleteItem(self, addOK, costBW, item):
        now = time.time()
        # After 10 secs without adding to any kworker, we will delete the fsItem
        agingTime = 10
        if 'restBW' not in item.keys():
            item.setdefault('restBW', item["bw_wr"])
            item.setdefault('startAging', now)
        if addOK:
            item["startAging"] = time.time()
        item["restBW"] = item["restBW"] - costBW if addOK else item["restBW"]
        if item["restBW"] <= 0 or (item["restBW"] < item["bw_wr"] and \
            (now - item["startAging"]) >= agingTime):
            return True
        return False


    def _miscIostatFromFsstat(self):
        fsstats = self._fsstat
        iostats = dict(self._iostat)
        for disk, fsItems in fsstats:
            if disk not in iostats.keys():
                continue
            rmList = []
            iostat = dict(iostats[disk])
            kworker = [key for key,val in iostat.items() if 'kworker' in val['comm']]
            for key, item in fsItems:
                taskI = item["pid"]+':'+disk
                if taskI in iostat.keys():
                    if 'file' not in iostat[taskI].keys():
                        iostat[taskI].setdefault('file', [])
                        iostat[taskI]['cid'] = item['cid']
                    iostat[taskI]["file"].append(item["file"])
                    if item["bw_wr"] <= (iostat[taskI]["bps_wr"] * 15):
                        rmList.append((key, item))
                    continue
                if kworker:
                    if item["bw_wr"] < item["bw_rd"]:
                        rmList.append((key, item))
                        continue
                    addOK,cost = self._addBioToKworker(iostat, kworker, item)
                    deleted = self._checkDeleteItem(addOK, cost, item)
                    if deleted:
                        rmList.append((key, item))
            for key, item in rmList:
                fsItems.remove((key, item))
            iostats[disk] = iostat
        return iostats


    def _miscShowJson(self, iostats):
        secs = self.io.cycle
        statJsonStr = '{"time":"","mstats":[]}'
        mstatDicts = json.loads(statJsonStr, object_pairs_hook=OrderedDict)
        mstatDicts['time'] = time.strftime('%Y/%m/%d %H:%M:%S', time.localtime())
        stSecs = str(secs)+'s' if secs > 1 else 's'

        for key, item in iostats:
            if (item["iops_rd"]+item["iops_wr"]) == 0 or (item["bps_rd"]+item["bps_wr"]) == 0:
                continue
            item["bps_rd"] = humConvert(
                item["bps_rd"], True).replace('s', stSecs) if item["bps_rd"] else '0'
            item["bps_wr"] = humConvert(
                item["bps_wr"], True).replace('s', stSecs) if item["bps_wr"] else '0'
            if 'file' not in item.keys():
                item.setdefault('file', '-')
            if 'kworker' in item["comm"] and 'bufferio' in item.keys():
                for i in item["bufferio"]:
                    i["Wrbw"] = humConvert(i["Wrbw"], True).replace('s', stSecs)
            mstatDicts["mstats"].append(item)
        if len(mstatDicts["mstats"]) > 0:
            self.io.writeDataToJson(json.dumps(mstatDicts))


    def miscShow(self):
        secs = self.io.cycle
        if not self._fsstat and not self._iostat:
            return

        iostats = self._miscIostatFromFsstat()
        if not iostats:
            return
        tmp = {}
        for d in iostats.values():
            tmp.update(dict(d))
        iostats = sorted(
            tmp.items(),
            key=lambda e: (int(e[1]["bps_rd"])+int(e[1]["bps_wr"])),
            reverse=True)
        if self.io.enableJsonShow() == True:
            self._miscShowJson(iostats)
            return

        print('%-20s%-8s%-24s%-12s%-16s%-12s%-12s%-8s%s' %
              ("comm", "pid", "cid", "iops_rd", "bps_rd", "iops_wr", "bps_wr",
              "device", "file"))
        stSecs = str(secs)+'s' if secs > 1 else 's'
        for key, item in iostats:
            if (item["iops_rd"]+item["iops_wr"]) == 0 or (item["bps_rd"]+item["bps_wr"]) == 0:
                continue
            item["bps_rd"] = humConvert(
                item["bps_rd"], True).replace('s', stSecs) if item["bps_rd"] else '0'
            item["bps_wr"] = humConvert(
                item["bps_wr"], True).replace('s', stSecs) if item["bps_wr"] else '0'
            file = str(item["file"]) if 'file' in item.keys() else '-'
            print('%-20s%-8s%-24s%-12s%-16s%-12s%-12s%-8s%s' %
                (item["comm"], str(item["pid"]), item["cid"][0:20], str(item["iops_rd"]),
                item["bps_rd"], str(item["iops_wr"]), item["bps_wr"], item["device"], file))
            if 'kworker' in item["comm"] and 'bufferio' in item.keys():
                for i in item["bufferio"]:
                    i["Wrbw"] = humConvert(i["Wrbw"], True).replace('s', stSecs)
                    print('  |----%-32s WrBw:%-12s Device:%-8s File:%s' %
                        (i['task'], i["Wrbw"], i["device"], i["file"]))
        print("")


    def config(self):
        self.fs.config()
        self.io.config()

    def start(self):
        self.clear()
        self.fs.start()
        self.io.start()

    def stop(self):
        self.fs.stop()
        self.io.stop()

    def clear(self):
        del self._iostat[:]

    def show(self):
        self.fs.show()
        self.io.show()
        self.miscShow()

    def entry(self, interval):
        self.start()
        time.sleep(float(interval))
        self.stop()
        self.show()
