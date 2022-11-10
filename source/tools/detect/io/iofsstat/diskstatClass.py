#!/usr/bin/python
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

def getDevtRegion(devname):
    if os.path.exists('/sys/block/'+devname):
        isPart = False
    elif os.path.exists('/sys/class/block/'+devname):
        isPart = True
    else:
        return [-1, -1]

    master = devname if not isPart else \
        os.readlink('/sys/class/block/'+devname).split('/')[-2]
    partList = list(
        filter(lambda x: master in x,
        os.listdir('/sys/class/block/'+master)))
    if not partList:
        partList = []
    partList.append(master)
    return [getDevt(p) for p in partList]
    

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
    def __init__(self, devname, utilThresh, json, nodiskStat, Pattern):
        self.devname = devname
        self.json = json
        self.cycle = 1
        self.started = False
        self.Pattern = Pattern
        self.nodiskStat = nodiskStat
        self.utilThresh = int(utilThresh) if utilThresh is not None else 0
        self.fieldDicts = OrderedDict()
        self.diskInfoDicts = {}
        self.deviceStatDicts = {}
        self.f = open("/proc/diskstats")
        if json:
            self.fJson = open(json, 'w+')


    def getDevNameByDevt(self, devt):
        try:
            return self.diskInfoDicts[str(devt)]['partName']
        except Exception:
            return '-'

    def getMasterDev(self, devt):
        try:
            return self.diskInfoDicts[str(devt)]['master']
        except Exception:
            return '-'

    def getDiskStatInd(self, disk, key):
        return self.deviceStatDicts[disk][key]

    def start(self):
        fieldDicts = self.fieldDicts
        diskInfoDicts = self.diskInfoDicts
        deviceStatDicts = self.deviceStatDicts

        if self.started:
            return
        self.started = True
        self.cycle = time.time()
        self.f.seek(0)
        for stat in self.f.readlines():
            stat = stat.split()
            if self.devname is not None and self.devname not in stat[2] and \
                stat[2] not in self.devname:
                continue

            field = {\
                '1':[0,0], '2':[0,0], '3':[0,0], '4':[0,0],\
                '5':[0,0], '6':[0,0], '7':[0,0], '8':[0,0],\
                '10':[0,0], '11':[0,0]}
            for idx,value in field.items():
                value[0] = long(stat[int(idx)+2])
            if stat[2] not in fieldDicts.keys():
                fieldDicts.setdefault(stat[2], field)
                path = os.readlink('/sys/class/block/'+stat[2]).split('/')
                master = path[-2]
                if master not in path[-1]:
                    master = path[-1]
                diskInfoDicts.setdefault(
                    str((int(stat[0])<<20)+int(stat[1])),
                    {'partName': stat[2], 'master': master})
                deviceStat = {
                    'r_rqm':0, 'w_rqm':0, 'r_iops':0, 'w_iops':0, 'r_bps':0,
                    'w_bps':0, 'wait':0, 'r_wait':0, 'w_wait':0, 'util%':-1}
                deviceStatDicts.setdefault(stat[2], deviceStat)
            else:
                deviceStatDicts[stat[2]]['util%'] = -1
                fieldDicts[stat[2]].update(field)

    def stop(self):
        fieldDicts = self.fieldDicts
        deviceStatDicts = self.deviceStatDicts
        self.cycle = max(int(time.time()-self.cycle), 1)

        if not self.started:
            return
        self.started = False

        self.f.seek(0)
        for stat in self.f.readlines():
            stat = stat.split()
            if self.devname is not None and self.devname not in stat[2] and \
                stat[2] not in self.devname:
                continue
            for idx,value in fieldDicts[stat[2]].items():
                value[1] = long(stat[int(idx)+2])

        for devname,field in fieldDicts.items():
            if self.devname is not None and devname not in self.devname and \
                self.devname not in devname:
                continue
            util = round((field['10'][1]-field['10'][0])*100.0/(self.cycle*1000),2)
            util = util if util <= 100 else 100.0
            if util < self.utilThresh:
                continue
            deviceStatDicts[devname]['util%'] = util
            r_iops = field['1'][1]-field['1'][0]
            deviceStatDicts[devname]['r_iops'] = r_iops
            r_rqm = field['2'][1]-field['2'][0]
            deviceStatDicts[devname]['r_rqm'] = r_rqm
            w_iops = field['5'][1]-field['5'][0]
            deviceStatDicts[devname]['w_iops'] = w_iops
            w_rqm = field['6'][1]-field['6'][0]
            deviceStatDicts[devname]['w_rqm'] = w_rqm
            r_bps = (field['3'][1]-field['3'][0]) * 512
            deviceStatDicts[devname]['r_bps'] = r_bps
            w_bps = (field['7'][1]-field['7'][0]) * 512
            deviceStatDicts[devname]['w_bps'] = w_bps
            r_ticks = field['4'][1]-field['4'][0]
            w_ticks = field['8'][1]-field['8'][0]
            wait = round((r_ticks+w_ticks)/(r_iops+w_iops), 2) if (r_iops+w_iops) else 0
            deviceStatDicts[devname]['wait'] = wait
            r_wait = round(r_ticks / r_iops, 2) if r_iops  else 0
            deviceStatDicts[devname]['r_wait'] = r_wait
            w_wait = round(w_ticks / w_iops, 2) if w_iops  else 0
            deviceStatDicts[devname]['w_wait'] = w_wait


    def __showJson(self):
        deviceStatDicts = self.deviceStatDicts
    
        statJsonStr = '{\
            "time":"",\
            "diskstats":[]}'
        dstatDicts = json.loads(statJsonStr, object_pairs_hook=OrderedDict)
        dstatDicts['time'] = time.strftime('%Y/%m/%d %H:%M:%S', time.localtime())
        for devname,stat in deviceStatDicts.items():
            if stat['util%'] < 0:
                continue
            dstatJsonStr = '{\
                "diskname":"","r_rqm":0,"w_rqm":0,"r_iops":0,"w_iops":0,\
                "r_bps":0,"w_bps":0,"wait":0,"r_wait":0,"w_wait":0,"util%":0}'
            dstatDict = json.loads(dstatJsonStr, object_pairs_hook=OrderedDict)
            dstatDict["diskname"] = devname
            for key,val in stat.items():
                dstatDict[key] = val
            dstatDicts["diskstats"].append(dstatDict)
        if len(dstatDicts["diskstats"]) > 0:
            data = json.dumps(dstatDicts)
            self.writeDataToJson(data)
            return

    def show(self):
        secs = self.cycle
        deviceStatDicts = self.deviceStatDicts
        if self.nodiskStat:
            return

        if self.enableJsonShow() == True:
            self.__showJson()
            return

        if self.Pattern:
            WrTotalIops = 0
            RdTotalIops = 0
            WrTotalBw = 0
            RdTotalBw = 0
        print('%-20s%-8s%-8s%-8s%-8s%-12s%-12s%-8s%-8s%-8s%-8s' %\
            (("device-stat:"),"r_rqm","w_rqm","r_iops","w_iops","r_bps",\
            "w_bps","wait","r_wait","w_wait","util%"))
        stSecs = str(secs)+'s' if secs > 1 else 's'
        for devname,stat in deviceStatDicts.items():
            if (not self.devname and not os.path.exists('/sys/block/'+devname)) or \
                stat['util%'] < 0:
                continue
            if self.Pattern:
                WrTotalIops += stat['w_iops']
                RdTotalIops += stat['r_iops']
                WrTotalBw += stat['w_bps']
                RdTotalBw += stat['r_bps']
            stWbps = humConvert(stat['w_bps'], True).replace('s', stSecs) if stat['w_bps'] else 0
            stRbps = humConvert(stat['r_bps'], True).replace('s', stSecs) if stat['r_bps'] else 0
            print('%-20s%-8s%-8s%-8s%-8s%-12s%-12s%-8s%-8s%-8s%-8s' %\
                (devname, str(stat['r_rqm']), str(stat['w_rqm']), str(stat['r_iops']),
                str(stat['w_iops']), stRbps, stWbps, str(stat['wait']), str(stat['r_wait']),
                str(stat['w_wait']), str(stat['util%'])))
        if self.Pattern:
            print('totalIops:%d(r:%d, w:%d), totalBw:%s(r:%s, w:%s)' %
                ((WrTotalIops+RdTotalIops), RdTotalIops, WrTotalIops,
                (humConvert((WrTotalBw+RdTotalBw), True).replace('s', stSecs) if (WrTotalBw+RdTotalBw > 0) else 0),
                (humConvert(RdTotalBw, True).replace('s', stSecs) if RdTotalBw else 0),
                (humConvert(WrTotalBw, True).replace('s', stSecs) if WrTotalBw else 0)))
        print("")

    def clear(self):
        self.f.close()
        if self.enableJsonShow():
            self.fJson.close()
    
    def notCareDevice(self, devname):
        if not self.nodiskStat and self.deviceStatDicts[devname]['util%'] < 0:
            return True
        return False
    
    def disableShow(self):
        deviceStatDicts = self.deviceStatDicts
        for devname,stat in deviceStatDicts.items():
            if self.deviceStatDicts[devname]['util%'] >= 0:
                return False
        return True
    
    def enableJsonShow(self):
        return True if self.json else False

    def writeDataToJson(self, data):
        self.fJson.write(data+'\n')
