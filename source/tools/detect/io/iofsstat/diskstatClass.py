#!/usr/bin/python2
# -*- coding: utf-8 -*-

import os
import time
import json
import string
from collections import OrderedDict


def getDevt(devname):
    try:
        with open('/sys/class/block/' + devname + '/dev') as f:
            dev = f.read().split(':')
            return ((int(dev[0]) << 20) + int(dev[1]))
    except Exception:
        return -1


def humConvert(value, withUnit):
    units = ["B", "KB", "MB", "GB", "TB", "PB"]
    size = 1024.0

    if value == 0:
        return value

    for i in range(len(units)):
        if (value / size) < 1:
            if withUnit:
                return "%.1f%s/s" % (value, units[i])
            else:
                return "%.1f" % (value)
        value = value / size


class diskstatClass(object):
    def __init__(self, devname, utilThresh, cycle, json):
        self.devname = devname
        self.json = json
        self.utilThresh = int(utilThresh) if utilThresh is not None else 0
        self.cycle = int(cycle) if cycle > 0 else 1
        self.fieldDicts = OrderedDict()
        self.diskInfoDicts = {}
        self.deviceStatDicts = {}
        self.f = open("/proc/diskstats")

    def getDevNameByDevt(self, devt):
        return self.diskInfoDicts[str(devt)]

    def start(self):
        fieldDicts = self.fieldDicts
        diskInfoDicts = self.diskInfoDicts
        deviceStatDicts = self.deviceStatDicts

        self.f.seek(0)
        for stat in self.f.readlines():
            stat = stat.split()
            if self.devname is not None and self.devname not in stat[2] and \
                    stat[2] not in self.devname:
                continue

            field = {
                '1': [0, 0], '3': [0, 0], '4': [0, 0],
                '5': [0, 0], '7': [0, 0], '8': [0, 0],
                '10': [0, 0], '11': [0, 0]}
            for idx, value in field.items():
                value[0] = long(stat[int(idx)+2])
            if stat[2] not in fieldDicts.keys():
                fieldDicts.setdefault(stat[2], field)
                diskInfoDicts.setdefault(
                    str((int(stat[0]) << 20)+int(stat[1])), stat[2])
                deviceStat = {
                    'r_iops': 0, 'w_iops': 0, 'r_bps': 0, 'w_bps': 0, 'wait': 0,
                    'r_wait': 0, 'w_wait': 0, 'util%': -1}
                deviceStatDicts.setdefault(stat[2], deviceStat)
            else:
                deviceStatDicts[stat[2]]['util%'] = -1
                fieldDicts[stat[2]].update(field)

    def stop(self):
        fieldDicts = self.fieldDicts
        deviceStatDicts = self.deviceStatDicts
        secs = self.cycle

        self.f.seek(0)
        for stat in self.f.readlines():
            stat = stat.split()
            if self.devname is not None and self.devname not in stat[2] and \
                    stat[2] not in self.devname:
                continue
            for idx, value in fieldDicts[stat[2]].items():
                value[1] = long(stat[int(idx)+2])

        for devname, field in fieldDicts.items():
            if self.devname is not None and devname != self.devname:
                continue
            util = round((field['10'][1]-field['10'][0])*100.0/(secs*1000), 2)
            util = util if util <= 100 else 100.0
            if util < self.utilThresh:
                continue
            deviceStatDicts[devname]['util%'] = util
            r_iops = round((field['1'][1]-field['1'][0]) / secs, 1)
            deviceStatDicts[devname]['r_iops'] = r_iops
            w_iops = round((field['5'][1]-field['5'][0]) / secs, 1)
            deviceStatDicts[devname]['w_iops'] = w_iops
            r_bps = (field['3'][1]-field['3'][0]) / secs * 512
            deviceStatDicts[devname]['r_bps'] = r_bps
            w_bps = (field['7'][1]-field['7'][0]) / secs * 512
            deviceStatDicts[devname]['w_bps'] = w_bps
            r_ticks = field['4'][1]-field['4'][0]
            w_ticks = field['8'][1]-field['8'][0]
            wait = round((r_ticks+w_ticks)/(r_iops+w_iops),
                         2) if (r_iops+w_iops) else 0
            deviceStatDicts[devname]['wait'] = wait
            r_wait = round(r_ticks / r_iops, 2) if r_iops else 0
            deviceStatDicts[devname]['r_wait'] = r_wait
            w_wait = round(w_ticks / w_iops, 2) if w_iops else 0
            deviceStatDicts[devname]['w_wait'] = w_wait

    def __showJson(self):
        deviceStatDicts = self.deviceStatDicts

        statJsonStr = '{\
			"time":"",\
			"diskstats":[]}'
        dstatDicts = json.loads(statJsonStr, object_pairs_hook=OrderedDict)
        dstatDicts['time'] = time.strftime(
            '%Y/%m/%d %H:%M:%S', time.localtime())
        for devname, stat in deviceStatDicts.items():
            if self.deviceStatDicts[devname]['util%'] < 0:
                continue
            dstatJsonStr = '{\
				"diskname":"",\
				"r_iops":0,\
				"w_iops":0,\
				"r_bps":0,\
				"w_bps":0,\
				"wait":0,\
				"r_wait":0,\
				"w_wait":0,\
				"util%":0}'
            dstatDict = json.loads(dstatJsonStr, object_pairs_hook=OrderedDict)
            dstatDict["diskname"] = devname
            for key, val in stat.items():
                dstatDict[key] = val
            dstatDicts["diskstats"].append(dstatDict)
        if len(dstatDicts["diskstats"]) > 0:
            print(json.dumps(dstatDicts))
            return

    def show(self):
        deviceStatDicts = self.deviceStatDicts

        if self.enableJsonShow() == True:
            self.__showJson()
            return

        print('%-20s%-8s%-8s%-12s%-12s%-8s%-8s%-8s%-8s' %
              (("device-stat:"), "r_iops", "w_iops", "r_bps",
               "w_bps", "wait", "r_wait", "w_wait", "util%"))
        for devname, stat in deviceStatDicts.items():
            if self.deviceStatDicts[devname]['util%'] < 0:
                continue
            print('%-20s%-8s%-8s%-12s%-12s%-8s%-8s%-8s%-8s' %
                  (devname, str(stat['r_iops']), str(stat['w_iops']), humConvert(stat['r_bps'], True),
                   humConvert(stat['w_bps'], True), str(stat['wait']), str(
                       stat['r_wait']), str(stat['w_wait']),
                   str(stat['util%'])))
        print("")

    def clear(self):
        self.f.close()

    def notCareDevice(self, devname):
        if self.deviceStatDicts[devname]['util%'] < 0:
            return True
        return False

    def disableShow(self):
        deviceStatDicts = self.deviceStatDicts
        for devname, stat in deviceStatDicts.items():
            if self.deviceStatDicts[devname]['util%'] >= 0:
                return False
        return True

    def getDiskBW(self):
        bw = 0
        deviceStatDicts = self.deviceStatDicts
        for devname, stat in deviceStatDicts.items():
            if self.deviceStatDicts[devname]['util%'] >= 0:
                bw += (int(stat['r_bps'])+int(stat['w_bps']))
        return bw

    def enableJsonShow(self):
        return self.json
