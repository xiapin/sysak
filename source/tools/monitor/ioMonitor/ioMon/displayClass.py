# -*- coding: utf-8 -*-

import os
import sys
import string
import time
import re
import json
import threading
from collections import OrderedDict
from nfPut import CnfPut


def bwToValue(bw):
    units = ["B", "KB", "MB", "GB", "TB", "PB"]
    if str(bw) == '0':
        return 0
    for i in range(5, -1, -1):
        if units[i] in bw:
            return float(bw.split(units[i])[0]) * pow(1024, i)


def humConvert(value):
    units = ["B", "KB", "MB", "GB", "TB", "PB"]
    size = 1024.0

    if value == 0:
        return value
    for i in range(len(units)):
        if (value / size) < 1:
            return "%.1f%s/s" % (value, units[i])
        value = value / size


def iolatencyResultReport(*argvs):
    result = []
    nf = argvs[1]
    nfPutPrefix = str(argvs[2])
    statusReportDicts = argvs[3]
    ioburst = False
    nfPrefix = []
    iolatStartT = statusReportDicts['iolatency']['startT']
    iolatEndT = statusReportDicts['iolatency']['endT']
    ioutilStartT = statusReportDicts['ioutil']['startT']
    ioutilEndT = statusReportDicts['ioutil']['endT']
    lastIOburstT = statusReportDicts['iolatency']['lastIOburstT']

    # If IO burst occurs in the short term(first 180 secs or next 60secs) or
    # during IO delay diagnosis, it should be considered as one of
    # the delay factors
    if iolatStartT - lastIOburstT < 300 or \
        (ioutilStartT >= (iolatStartT - 300) and ioutilEndT <= (iolatEndT + 60)):
        statusReportDicts['iolatency']['lastIOburstT'] = iolatStartT
        ioburst = True

    os.system('ls -rtd '+argvs[0]+'/../* | head -n -5 | '\
        'xargs --no-run-if-empty rm {} -rf')
    if os.path.exists(argvs[0]+'/result.log.stat'):
        with open(argvs[0]+'/result.log.stat') as logF:
            data = logF.readline()
    else:
        return
    try:
        stat = json.loads(data, object_pairs_hook=OrderedDict)
    except Exception:
        return

    for ds in stat['summary']:
        delays = sorted(ds['delays'],
                        key=lambda e: (float(e['percent'].strip('%'))),
                        reverse=True)
        maxDelayComp = delays[0]['component']
        maxDelayPercent = float(delays[0]['percent'].strip('%'))
        avgLat = format(sum([d['avg'] for d in delays])/1000.0, '.3f')
        diagret = 'diagret=\"IO delay(AVG %sms) detected in %s\"' % (
            avgLat, str(ds['diskname']))
        nfPrefix.append(',diag_type=IO-Delay,devname='+str(ds['diskname']))

        if ioburst and {maxDelayComp, delays[1]['component']}.issubset(
                ['disk', 'os(block)']):
            if (delays[0]['avg'] / delays[1]['avg']) < 10:
                suggest = 'solution=\"reduce IO pressure. Refer to the '\
                    'diagnosis of IO-Burst and optimize some tasks\"'
                diskIdx = 0
                if maxDelayComp == 'os(block)':
                    diskIdx = 1
                reason = (
                    'reason=\"IO burst occurs, too mang IO backlogs'\
                    '(disk avg/max lat:%s/%s ms, lat percent:%s,'\
                    ' OS dispatch avg/max lat:%s/%s ms, lat percent:%s\"' %
                        (str(delays[diskIdx]['avg'] / 1000.000),
                        str(delays[diskIdx]['max'] / 1000.000),
                        str(delays[diskIdx]['percent']),
                        str(delays[1 - diskIdx]['avg'] / 1000.000),
                        str(delays[1 - diskIdx]['max'] / 1000.000),
                        str(delays[1 - diskIdx]['percent'])))
                result.append(diagret+','+reason+','+suggest)
                continue
            else:
                statusReportDicts['iolatency']['lastIOburstT'] = lastIOburstT

        suggest = 'solution=\"Please ask the OS kernel expert\"'
        maxDelayLog = 'avg/max lat:%s/%s ms, lat percent:%s' %(
            str(delays[0]['avg']/1000.000),
            str(delays[0]['max']/1000.000),
            str(delays[0]['percent']))
        if maxDelayComp == 'disk':
            reason = (
                'reason=\"Disk delay(processing IO slowly, %s)\"' %(maxDelayLog))
            suggest = 'solution=\"Please confirm whether the disk is normal\"'
        elif maxDelayComp == 'os(block)':
            if delays[1]['component'] == 'disk' and \
                float(delays[1]['percent'].strip('%')) > 20:
                with open(argvs[0]+'/resultCons.log') as logF:
                    data = filter(lambda l : 'F' in l, logF.readlines())
                flushIO = False
                if len(data) > 0:
                    for d in data:
                        if 'F' in d.split()[-6]:
                            flushIO = True
                            break
                if flushIO:
                    suggest = (
                        'Disable flush IO dispatch(echo \"write through\" > '\
                        '/sys/class/block/%s/queue/write_cache;'\
                        'echo 0 > /sys/class/block/%s/queue/fua)}' % (
                            str(ds['diskname']), str(ds['diskname'])))
                    suggest += '; Notes: Flush IO is a special instruction to '\
                        'ensure that data is stored persistently on the disk '\
                        'in time, and not saved in the internal cache of the disk.'\
                        ' Before disabling, please confirm with the disk FAE '\
                        '\"Whether it is necessary to rely on the software to issue'\
                        ' flush instructions to ensure data persistent storage\",'\
                        ' And avoid data loss due to crash or disk power down'
                    suggest = 'solution=\"'+suggest+'\"'
                else:
                    suggest = 'solution=\"Please confirm whether the disk is normal\"'
                reason = (
                    'reason=\"Disk delay(processing %s slowly, avg/max lat:'\
                    '%s/%s ms, lat percent:%s)\"' %(
                        'Flush IO' if flushIO else 'IO',
                        str(delays[1]['avg']/1000.000),
                        str(delays[1]['max']/1000.000),
                        str(delays[1]['percent'])))
                result.append(diagret+','+reason+','+suggest)
                continue
            reason = (
                'reason=\"OS delay(Issuing IO slowly at os(block), %s)\"' %(
                    maxDelayLog))
        else:
            reason = (
                'reason=\"OS delay(processing IO slowly at %s, %s)\"' %(
                    str(maxDelayComp), maxDelayLog))
        result.append(diagret+','+reason+','+suggest)

    for e, p in zip(result, nfPrefix):
        # print(e+'\n')
        #nf.put(nfPutPrefix, p+' '+e)
        nf.puts(nfPutPrefix+p+' '+e)
        statusReportDicts['iolatency']['valid'] = True


def iohangResultReport(*argvs):
    abnormalDicts={}
    firstioDicts={}
    result=[]
    nf=argvs[1]
    nfPutPrefix=str(argvs[2])
    statusReportDicts = argvs[3]
    nfPrefix=[]

    os.system('ls -rtd '+argvs[0]+'/../* | head -n -5 |'\
        ' xargs --no-run-if-empty rm {} -rf')
    if os.path.exists(argvs[0]+'/result.log'):
        with open(argvs[0]+'/result.log') as logF:
            data=logF.readline()
    else:
        return
    try:
        stat=json.loads(data, object_pairs_hook = OrderedDict)
    except Exception:
        return

    for ds in stat['summary']:
        maxDelay = 0
        hungIO = None
        if ds['diskname'] not in abnormalDicts.keys():
            abnormalDicts.setdefault(ds['diskname'], {})
            firstioDicts.setdefault(
                ds['diskname'],
                {'time':0, 'iotype':0, 'sector':0})
        for hi in ds['hung ios']:
            key=hi['abnormal'].split('hang')[0]
            delay = float(hi['abnormal'].split('hang')[1].split()[0])
            if delay > maxDelay:
                maxDelay = delay
                hungIO = hi
            if key not in abnormalDicts[ds['diskname']].keys():
                abnormalDicts[ds['diskname']].setdefault(key, 0)
            abnormalDicts[ds['diskname']][key] += 1
        t = hungIO['time'].split('.')[0]
        tStamp = float(time.mktime(time.strptime(t,'%Y-%m-%d %H:%M:%S')))
        tStamp -= maxDelay
        firstioDicts[ds['diskname']]['time'] = \
            time.strftime('%Y/%m/%d %H:%M:%S', time.localtime(tStamp+8*3600))
        firstioDicts[ds['diskname']]['iotype'] = hungIO['iotype']
        firstioDicts[ds['diskname']]['sector'] = hungIO['sector']
    for diskname, val in abnormalDicts.items():
        abnormalDicts[diskname] = OrderedDict(
            sorted(val.items(), key=lambda e: e[1], reverse=True))

    with open(argvs[0]+'/result.log.stat') as logF:
        data = logF.readline()
    try:
        stat = json.loads(data, object_pairs_hook=OrderedDict)
    except Exception:
        return

    for ds in stat['summary']:
        hungIOS = sorted(ds['hung ios'],
                         key = lambda e: (float(e['percent'].strip('%'))),
                         reverse = True)
        maxDelayComp=hungIOS[0]['component']
        maxDelayPercent=float(hungIOS[0]['percent'].strip('%'))
        maxDelay=format(hungIOS[0]['max']/1000.0, '.3f')
        diagret='diagret=\"IO hang %sms detected in %s' % (
            maxDelay, ds['diskname'])+'\"'
        nfPrefix.append(',diag_type=IO-Hang,devname='+str(ds['diskname']))
        for key in abnormalDicts[ds['diskname']].keys():
            if maxDelayComp in key:
                detail = str(
                    ''.join(re.findall(re.compile(r'[(](.*?)[)]', re.S), key)))
                break
        reason = ('reason=\"%s hang(%s, avg/max delay:%s/%s ms), first hang['\
            'time:%s, iotype:%s, sector:%d]\"' %(
            maxDelayComp, detail,
            str(hungIOS[0]['avg']/1000.000),
            str(hungIOS[0]['max']/1000.000),
            firstioDicts[ds['diskname']]['time'],
            firstioDicts[ds['diskname']]['iotype'],
            firstioDicts[ds['diskname']]['sector']))
        if maxDelayComp == 'Disk' or maxDelayComp == 'OS':
            suggest = 'solution=\"Please confirm whether the disk is normal\"'
            if maxDelayComp == 'OS':
                suggest = 'solution=\"Please ask the OS kernel expert\"'
            result.append(diagret+','+reason+','+suggest)

    for e, p in zip(result, nfPrefix):
        nf.puts(nfPutPrefix+p+' '+e)
        #nf.put(nfPutPrefix, p+' '+e)
        statusReportDicts['iohang']['valid'] = True


def ioutilDataParse(data, resultInfo):
    tUnit = None
    totalBw = totalIops = 0
    for ds in data['mstats']:
        iops = ds['iops_rd'] + ds['iops_wr']
        bps = bwToValue(ds['bps_wr']) + bwToValue(ds['bps_rd'])
        totalBw += bps
        totalIops += iops
        key = ds['comm']+':'+ds['pid']+':'+ds['cid'][0:20]+':'+ds['device']
        if not tUnit:
            if ds['bps_wr'] != '0':
                tUnit = ds['bps_wr'].split('/')[1]
            else:
                tUnit = ds['bps_rd'].split('/')[1]
        if key not in resultInfo.keys():
            resultInfo.setdefault(key,
                {'disk':ds['device'], 'maxIops':0, 'maxBps':0, 'file':ds['file']})
        resultInfo[key]['maxBps'] = max(bps, resultInfo[key]['maxBps'])
        resultInfo[key]['maxIops'] = max(iops, resultInfo[key]['maxIops'])
        if resultInfo[key]['maxBps'] != bps or resultInfo[key]['maxIops'] != iops:
            resultInfo[key]['file'] = ds['file']
            if 'bufferio' in resultInfo.keys():
                del resultInfo[key]['bufferio']
        if 'bufferio' in ds.keys() and 'bufferio' not in resultInfo[key].keys():
            resultInfo[key].setdefault('bufferio', ds['bufferio'])
    return totalIops,totalBw,tUnit


def ioutilReport(nf, nfPutPrefix, resultInfo, tUnit, diagret):
    top = 1
    suggestPS = reason = ''
    resultInfo = \
        sorted(resultInfo.items(), key=lambda e: e[1]['maxBps'], reverse=True)
    for key, val in resultInfo:
        if val['maxIops'] < 50 or val['maxBps'] < 1024 * 1024 * 5:
            continue
        file = ', target file:'+str(val['file']) if val['file'] != '-' else ''
        if 'kworker' in str(key):
            kTasklist = []
            if 'bufferio' in val.keys():
                for i in val["bufferio"]:
                    if 'KB' in i["Wrbw"]:
                        continue
                    kTasklist.append(i['task'])
                    file += ('%s Wrbw %s disk %s file %s;' %
                        (i['task'], i["Wrbw"], i["device"], i["file"]))
            if len(kTasklist):
                file = '(Write bio from: '+file+')'
            if top == 1:
                suggestPS = '(Found \'kworker\' flush dirty pages, Try to reduce'\
                    ' the buffer-IO write?%s or check the config /proc/sys/vm/'\
                    '{dirty_ratio,dirty_background_ratio} too small?)' %(
                        '('+';'.join(kTasklist)+')' if len(kTasklist) else '')
        maxBps = humConvert(val['maxBps']).replace('s', tUnit)
        reason += ('%d. task[%s], access disk %s with iops:%s, bps:%s%s; ' %(
            top, str(key.rsplit(':',1)[0]), str(val['disk']),
            str(val['maxIops']), maxBps, file))
        if top == 1 and suggestPS == '':
            suggestPS = '(Found task \'%s\')' %(str(key.rsplit(':',1)[0]))
        top += 1
    suggest = \
        'Optimize the tasks that contributes the most IO flow%s' % suggestPS
    putIdx = ',diag_type=IO-Burst '
    putField = 'diagret=\"%s\",reason=\"%s\",solution=\"%s\"' %(
        diagret, reason, suggest)
    #nf.put(nfPutPrefix,
    if reason != '':
        nf.puts(nfPutPrefix+putIdx+putField)
    # print(prefix+reason+suggest+'\n')


def ioutilResultReport(*argvs):
    resultInfo= {}
    nf= argvs[1]
    nfPutPrefix= str(argvs[2])
    statusReportDicts = argvs[3]
    totalBw = 0
    maxIops = maxBw = 0
    minIops = minBw = sys.maxsize
    tUnit = None

    os.system('ls -rtd '+os.path.dirname(argvs[0])+'/../* | head -n -5 |'\
        ' xargs --no-run-if-empty rm {} -rf')
    if os.path.exists(argvs[0]):
        with open(argvs[0]) as logF:
            dataList = logF.readlines()
    else:
        return
    for data in dataList:
        try:
            stat = json.loads(data, object_pairs_hook =OrderedDict)
        except Exception:
            return
        iops,bw,tUnit = ioutilDataParse(stat, resultInfo)
        maxIops = max(maxIops, iops)
        minIops = min(minIops, iops)
        maxBw = max(maxBw, bw)
        minBw = min(minBw, bw)
        totalBw += bw
    if totalBw < 1024 * 1024 * 10:
        return

    if resultInfo:
        content = 'Iops:'+str(minIops)+'~'+str(maxIops)+\
            ', Bps:'+humConvert(minBw).replace('s', tUnit)+\
            '~'+humConvert(maxBw).replace('s', tUnit)
        diagret = 'IO-Burst('+content+') detected'
        ioutilReport(nf, nfPutPrefix, resultInfo, tUnit, diagret)
        statusReportDicts['ioutil']['valid'] = True


def iowaitDataParse(data, resultInfo):
    unkownDisable = False
    for io in data['iowait']:
        if 'many dirty' in io['reason'] or 'queue full' in io['reason']:
            unkownDisable = True
        if 'Unkown' in io['reason'] and unkownDisable == True:
            continue
        key = io['comm']+':'+io['tgid']+':'+io['pid']
        if key not in resultInfo.keys():
            resultInfo.setdefault(
                key, {'timeout': 0, 'maxIowait': 0, 'reason': ''})
        if float(io['iowait']) > float(resultInfo[key]['maxIowait']):
            resultInfo[key]['maxIowait'] = io['iowait']
            resultInfo[key]['timeout'] = io['timeout']
            resultInfo[key]['reason'] = io['reason']
    return data['global iowait'],unkownDisable


def iowaitReport(nf, nfPutPrefix, unkownDisable, resultInfo, diagret):
    top = 0
    reason = ''
    resDicts = {
        'Too many dirty pages':False,
        'Device queue full':False,
        'Ioscheduler queue full':False}

    for key, val in resultInfo.items():
        if unkownDisable == True and 'Unkown' in val['reason']:
            del resultInfo[key]

    resultInfo = OrderedDict(
        sorted(resultInfo.items(), key=lambda e: float(e[1]['maxIowait']),
        reverse=True)[:3])
    for key, val in resultInfo.items():
        if unkownDisable == True:
            resDicts[val['reason']] = True
        top += 1
        reason += (
            '%d. task[%s], wait %sms, contribute iowait %s due to \'%s\'; ' %(
                top, str(key), str(val['timeout']), str(val['maxIowait'])+'%',
                str(val['reason'])))

    if unkownDisable == True:
        if resDicts['Too many dirty pages'] == True:
            suggest = 'Reduce io-write pressure or Adjust /proc/sys/vm/'\
                '{dirty_ratio,dirty_bytes} larger carefully'
        else:
            if resDicts['Device queue full'] and resDicts['Ioscheduler queue full']:
                suggest = \
                    'Device queue full -> Disk busy due to disk queue full, '\
                    'Please reduce io pressure;'\
                    'Ioscheduler queue full -> Io scheduler busy due to '\
                    'scheduler queue full, '\
                    'Please reduce io pressure or Adjust '\
                    '/sys/block/<disk-device>/queue/nr_requests larger carefully'
            elif resDicts['Device queue full']:
                suggest = 'Disk busy due to disk queue full, '\
                    'Please reduce io pressure'
            elif resDicts['Ioscheduler queue full']:
                suggest = 'Io scheduler busy due to scheduler queue full, '\
                    'Please reduce io pressure or Adjust '\
                    '/sys/block/<disk-device>/queue/nr_requests larger carefully'
    else:
        suggest = 'Report stacktrace to OS kernel specialist'

    putIdx = ',diag_type=IOwait-high '
    putField = 'diagret=\"%s\",reason=\"%s\",solution=\"%s\"' %(
        diagret, reason, suggest)
    #nf.put(nfPutPrefix,
    nf.puts(nfPutPrefix+putIdx+putField)


def iowaitResultReport(*argvs):
    resultInfo = {}
    nf = argvs[1]
    nfPutPrefix = str(argvs[2])
    statusReportDicts = argvs[3]
    maxGiowait = 0
    minGiowait = sys.maxsize
    unkownDisable = None

    os.system('ls -rtd '+os.path.dirname(argvs[0])+'/../* | head -n -5 |'\
        ' xargs --no-run-if-empty rm {} -rf')
    if os.path.exists(argvs[0]):
        with open(argvs[0]) as logF:
            dataList = logF.readlines()
    else:
        return

    for data in dataList:
        try:
            stat = json.loads(data, object_pairs_hook=OrderedDict)
        except Exception:
            return
        gIowait,disable = iowaitDataParse(stat, resultInfo)
        if not unkownDisable:
            unkownDisable = disable
        maxGiowait = max(maxGiowait, gIowait)
        minGiowait = min(minGiowait, gIowait)

    if resultInfo:
        content = str(minGiowait)+'%~'+str(maxGiowait)+'%'
        diagret = 'IOwait high('+content+') detected'
        iowaitReport(nf, nfPutPrefix, unkownDisable, resultInfo, diagret)
        statusReportDicts['iowait']['valid'] = True
        # print(diagret+reason+solution+'\n')


class displayClass(object):
    def __init__(self, sender):
        self.funcResultReportDicts = {
            'iohang': iohangResultReport,
            'ioutil': ioutilResultReport,
            'iolatency': iolatencyResultReport,
            'iowait': iowaitResultReport}
        self.statusReportDicts = {
            'iohang': {'startT': 0, 'endT': 0, 'valid': False},
            'ioutil': {'startT': 0, 'endT': 0, 'valid': False,
                    'iopsThresh': 0, 'bpsThresh': 0},
            'iolatency': {'startT': 0, 'endT': 0, 'valid': False,
                    'lastIOburstT': 0},
            'iowait': {'startT': 0, 'endT': 0, 'valid': False},
        }
        self._sender = sender
        self._nfPutPrefix = 'IOMonDiagLog'

    def markIoburst(self, now):
        self.statusReportDicts['iolatency']['lastIOburstT'] = now
    
    def setIoburstThresh(self, iopsThresh, bpsThresh):
        self.statusReportDicts['ioutil']['iopsThresh'] = iopsThresh
        self.statusReportDicts['ioutil']['bpsThresh'] = bpsThresh

    def diagnoseValid(self, diagType):
        return self.statusReportDicts[diagType]['valid']

    def start(self, timeout, diagType, filepath, startTime, endTime):
        self.statusReportDicts[diagType]['startT'] = startTime
        self.statusReportDicts[diagType]['endT'] = endTime
        self.statusReportDicts[diagType]['valid'] = False
        argvs = [
            filepath, self._sender, self._nfPutPrefix, self.statusReportDicts]
        timer = threading.Timer(timeout,
                                self.funcResultReportDicts[diagType],
                                argvs)
        timer.start()
