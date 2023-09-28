# -*- coding: utf-8 -*-

import os
import sys
import signal
import string
import argparse
import time
from exceptDiagnoseClass import diagnoseClass
from exceptCheckClass import exceptCheckClass
from ioMonCfgClass import ioMonCfgClass
from collections import OrderedDict
from nfPut import CnfPut

class ioMonitorClass(object):
    def __init__(self, logRootPath, cfg, pipeFile):
        self.window = 60
        self.cfg = cfg
        self.cfg.createCfgFlagFile()
        self.diagSwitch = {
            'diagIowait': {'sw': self.cfg.getCfgItem('diagIowait'),
                            'esi':'IOwait-High'},
            'diagIoburst': {'sw': self.cfg.getCfgItem('diagIoburst'),
                            'esi':'IO-Delay'},
            'diagIolat': {'sw': self.cfg.getCfgItem('diagIolat'),
                            'esi':'IO-Burst'},
            'diagIohang': {'sw': self.cfg.getCfgItem('diagIohang'),
                            'esi':'IO-Hang'}
            }
        self._sender = CnfPut(pipeFile)
        self._nfPutTlb = 'IOMonIndForDisksIO'
        self._nfPutTlb4System = 'IOMonIndForSystemIO'
        self.fieldDicts = OrderedDict()
        self.exceptChkDicts = {'system': exceptCheckClass(self.window)}
        self.exceptChkDicts['system'].addItem('iowait')
        self.diagnose = diagnoseClass(self.window, logRootPath, self._sender)
        self.diagnose.addItem('system', 'iowait', 0, 60)
        self.fDiskStats = open("/proc/diskstats")
        self.cpuStatIowait = {'sum': 0, 'iowait': 0}
        self.uploadInter = 0
        self.exceptionStat = {'system': {'IOwait-High': {'cur':0,'max':0}}}
        self.dataStat = {'system': {'iowait': 0}}


    def _addMonitorAttrForDisk(self, disk):
        dataStat = self.dataStat
        exceptChkDicts = self.exceptChkDicts
        diagnose = self.diagnose
        exceptionStat = self.exceptionStat

        # used for reporting per-index to database
        dataStat.setdefault(
            disk, {'await': 0, 'util': 0, 'iops': 0, 'bps': 0, 'qusize': 0})

        # add exception-check attr for per-index on per-disk
        exceptChkDicts.setdefault(disk, exceptCheckClass(self.window))
        for key in ['util', 'await', 'iops', 'bps']:
            exceptChkDicts[disk].addItem(key)

        # add diagnose attr for per-index on per-disk
        diagnoseDict = {
            'iohang': {'triggerInterval': self.window * 5,
                        'reportInterval': 10},
            'ioutil': {'triggerInterval': 60, 'reportInterval': 0},
            'iolatency': {'triggerInterval': 60, 'reportInterval': 0}
            }
        for key, item in diagnoseDict.items():
            diagnose.addItem(
                disk, key, item['reportInterval'], item['triggerInterval'])
            # used for reporting exception to database
            exceptionStat.setdefault(
                disk,
                {'IO-Delay':{'cur':0,'max':0}, 'IO-Burst':{'cur':0,'max':0},
                'IO-Hang':{'cur':0,'max':0}})


    def _removeDiskMonitor(self, disk):
        del self.fieldDicts[disk]
        del self.dataStat[disk]
        del self.exceptChkDicts[disk]
        del self.diagnose[disk]
        del self.exceptionStat[disk]


    def _disableThreshComp(self, devname, qusize):
        exceptChkDicts = self.exceptChkDicts
        exceptChkDicts[devname].disableThreshComp('util')
        exceptChkDicts[devname].disableThreshComp('iops')
        exceptChkDicts[devname].disableThreshComp('bps')
        if qusize > 1:
            exceptChkDicts[devname].disableThreshComp('await')


    def _calcIowait(self):
        with open("/proc/stat") as fStat:
            statList = map(long, fStat.readline().split()[1:])
        iowait = float(format(
            (statList[4] - self.cpuStatIowait['iowait']) * 100.0 /
            (sum(statList) - self.cpuStatIowait['sum']), '.2f'))
        return iowait
    

    def _calcIoIndex(self, devname, field, secs):
        ds = self.dataStat[devname]
        uploadInter = self.uploadInter

        rws = field['1'][1] + field['5'][1] - field['1'][0] - field['5'][0]
        iops = round(rws / secs, 1)
        ds['iops'] = (ds['iops'] * (uploadInter - 1) + iops) / uploadInter

        rwSecs = field['3'][1] + field['7'][1] - field['3'][0] - field['7'][0]
        bps = rwSecs / secs * 512
        ds['bps'] = (ds['bps'] * (uploadInter - 1) + bps) / uploadInter

        qusize = round((field['11'][1] - field['11'][0]) / secs / 1000, 2)
        ds['qusize'] = (ds['qusize'] * (uploadInter - 1) + qusize) / uploadInter

        rwTiks = field['4'][1] + field['8'][1] - field['4'][0] - field['8'][0]
        wait = round(rwTiks / (iops * secs), 2) if iops else 0
        ds['await'] = (ds['await'] * (uploadInter - 1) + wait) / uploadInter

        times = field['10'][1] - field['10'][0]
        util = round(times * 100.0 / (secs * 1000), 2)
        util = util if util <= 100 else 100.0
        ds['util'] = (ds['util'] * (uploadInter - 1) + util) / uploadInter

        return {'iops': iops*secs, 'bps': bps*secs, 'qusize': qusize*secs,
            'wait': wait, 'util': util}


    def _checkDiagSwitchChange(self, t):
        s = self.diagSwitch[t]
        newSW = self.cfg.getCfgItem(t)
        if newSW != s['sw'] and newSW == 'on':
            for k,v in self.exceptionStat.items():
                if s['esi'] in v.keys():
                    v[s['esi']]['max'] = 0
        s['sw'] = newSW
        return newSW


    def _checkIOwaitException(self, iowait):
        exceptChk = self.exceptChkDicts['system']
        dataStat = self.dataStat['system']
        es = self.exceptionStat['system']['IOwait-High']
        diagnose = self.diagnose
        uploadInter = self.uploadInter

        if iowait >= self.cfg.getCfgItem('iowait'):
            exceptChk.disableThreshComp('iowait')

        dataStat['iowait'] = \
            (dataStat['iowait'] * (uploadInter - 1) + iowait) / uploadInter

        # Detect iowait exception
        minThresh = self.cfg.getCfgItem('iowait')
        iowaitThresh = max(exceptChk.getDynThresh('iowait'), minThresh)
        if iowait >= iowaitThresh:
            es['cur'] += 1
            diagSW = self._checkDiagSwitchChange('diagIowait')
            rDiagValid = diagnose.recentDiagnoseValid('iowait')
            # Configure iowait diagnosis
            if diagSW == 'on' and (es['cur'] > es['max'] or not rDiagValid):
                nrSample = exceptChk.getNrDataSample('iowait')
                iowaitArg = max(int(iowait * 0.25), minThresh)
                diagnose.setUpDiagnose('system', 'iowait', nrSample, iowaitArg)

        exceptChk.updateDynThresh('iowait', iowait)

    def _checkIoburstException(self, devname, es, bps, iops, exceptChk):
        bpsLowW = self.cfg.getCfgItem('bps')
        bpsHighW = max(exceptChk.getDynThresh('bps'), bpsLowW)
        bpsMiddW = max(bpsLowW, bpsHighW / 2)
        iopsLowW = self.cfg.getCfgItem('iops')
        iopsHighW = max(exceptChk.getDynThresh('iops'), iopsLowW)
        iopsMiddW = max(iopsLowW, iopsHighW / 2)
        ioburst = exception = False

        if iops >= iopsMiddW or bps >= bpsMiddW:
            ioburst = True
        bpsOver = True if bps >= bpsHighW else False
        iopsOver = True if iops >= iopsHighW else False

        if iopsOver or bpsOver:
            es['cur'] += 1
            diagSW = self._checkDiagSwitchChange('diagIoburst')
            rDiagValid = self.diagnose.recentDiagnoseValid('ioutil')
            # Configure IO load diagnosis
            if diagSW == 'on' and (es['cur'] > es['max'] or not rDiagValid):
                bpsArg = iopsArg = 0
                if bpsOver == True:
                    bpsArg = max(int(bps * 0.25), bpsLowW)
                if iopsOver == True:
                    iopsArg = max(int(iops * 0.7), iopsLowW)
                nrSample = exceptChk.getNrDataSample('util')
                self.diagnose.setUpDiagnose(
                    devname, 'ioutil', nrSample, bpsArg, iopsArg)
        return ioburst

    def _checkIohangException(
        self, devname, es, util, qusize, iops, exceptChk):
        # Detect IO hang
        if util >= 100 and qusize >= 1 and iops < 50:
            # Configure IO hang diagnosis
            if self.diagnose.isException(devname, 'iohang') == True:
                es['cur'] += 1
            diagSW = self._checkDiagSwitchChange('diagIohang')
            rDiagValid = self.diagnose.recentDiagnoseValid('iohang')
            if diagSW == 'on' and (es['cur'] > es['max'] or not rDiagValid):
                nrSample = exceptChk.getNrDataSample('util')
                self.diagnose.setUpDiagnose(devname, 'iohang', nrSample)

    def _checkUtilException(self, devname, util, iops, bps, qusize):
        exceptChk = self.exceptChkDicts[devname]
        exceptionStat = self.exceptionStat[devname]
        diagnose = self.diagnose
        ioburst = False

        utilMinThresh = self.cfg.getCfgItem('util')
        utilThresh = max(exceptChk.getDynThresh('util'), utilMinThresh)
        if util >= utilThresh:
            es = exceptionStat['IO-Burst']
            ioburst = \
                self._checkIoburstException(devname, es, bps, iops, exceptChk)
            if not ioburst:
                es = exceptionStat['IO-Hang']
                self._checkIohangException(
                    devname, es, util, qusize, iops, exceptChk)                 
        exceptChk.updateDynThresh('util', util)
        exceptChk.updateDynThresh('iops', iops)
        exceptChk.updateDynThresh('bps', bps)
        return ioburst


    def _checkAwaitException(self, devname, wait, ioburst):
        exceptChk = self.exceptChkDicts[devname]
        es = self.exceptionStat[devname]['IO-Delay']
        diagnose = self.diagnose

        awaitMinThresh = self.cfg.getCfgItem('await')
        awaitThresh = max(exceptChk.getDynThresh('await'), awaitMinThresh)
        if wait >= awaitThresh:
            es['cur'] += 1
            diagSW = self._checkDiagSwitchChange('diagIolat')
            rDiagValid = diagnose.recentDiagnoseValid('iolatency')
            # Configuring IO delay diagnostics
            if diagSW == 'on' and (es['cur'] > es['max'] or not rDiagValid):
                nrSample = exceptChk.getNrDataSample('await')
                waitArg = max(int(wait * 0.4), awaitMinThresh)
                diagnose.setUpDiagnose(
                    devname, 'iolatency', nrSample, waitArg, ioburst)
        exceptChk.updateDynThresh('await', wait)


    def _reportDataToRemote(self, devList):
        # report datastat&&exception to database
        nCycle = 1000.0 / float(self.cfg.getCfgItem('cycle'))
        dataStat = self.dataStat['system']
        es = self.exceptionStat['system']['IOwait-High']

        putIdx = ',idx_type=system_Indicator,devname=system '
        putField = 'iowait=%f,iowaithighCnt=%f' %(
            dataStat['iowait'], es['cur'] / nCycle)
        self._sender.puts(self._nfPutTlb4System + putIdx + putField)

        es['max'] = max(es['max'] if es['cur'] else 0, es['cur'])
        es['cur'] = 0
        cur = {'IO-Delay':0, 'IO-Burst':0, 'IO-Hang':0}
        for devname in devList:
            dataStat = self.dataStat[devname]
            es = self.exceptionStat[devname]
            for type in cur.keys():
                cur[type] = int(es[type]['cur']) / nCycle
                es[type]['max'] = \
                    max(es[type]['max'] if cur[type] else 0, cur[type])
                es[type]['cur'] = 0

            putIdx = ',idx_type=iostat_Indicator,devname=%s ' % devname
            putField = 'await=%f,util=%f,iops=%f,bps=%f,qusize=%f' %(
                dataStat['await'], dataStat['util'], dataStat['iops'],
                dataStat['bps'] / 1024.0, dataStat['qusize']
            )
            putField += ',iodelayCnt=%f,ioburstCnt=%f,iohangCnt=%f' %(
                cur['IO-Delay'], cur['IO-Burst'], cur['IO-Hang'])
            self._sender.puts(self._nfPutTlb + putIdx + putField)


    def _collectBegin(self):
        fieldDicts = self.fieldDicts

        # collect iowait begin
        with open("/proc/stat") as fStat:
            cpuStatList = map(long, fStat.readline().split()[1:])
            self.cpuStatIowait['sum'] = sum(cpuStatList)
            self.cpuStatIowait['iowait'] = cpuStatList[4]
        
        # collect iostat begin
        self.fDiskStats.seek(0)
        for stat in self.fDiskStats.readlines():
            stat = stat.split()
            if os.path.exists('/sys/block/'+stat[2]) == False or stat[2].startswith("loop"):
                if stat[2] in fieldDicts.keys():
                    self._removeDiskMonitor(stat[2])
                continue

            if stat[2] in fieldDicts.keys():
                field = fieldDicts[stat[2]]
            else:
                field = {
                    '1': [0, 0], '3': [0, 0], '4': [0, 0],
                    '5': [0, 0], '7': [0, 0], '8': [0, 0],
                    '10': [0, 0], '11': [0, 0]}
                 # add data staticsis for per-disk
                fieldDicts.setdefault(stat[2], field)
                self._addMonitorAttrForDisk(stat[2])

            for idx, value in field.items():
                value[0] = long(stat[int(idx) + 2])


    def _collectEnd(self, secs):
        fieldDicts = self.fieldDicts
        exceptChkDicts = self.exceptChkDicts
        uploadInter = self.uploadInter
        
        self.uploadInter = \
            1 if ((uploadInter * secs) % 60) == 0 else (uploadInter + 1)

        # Calculate iowait
        iowait = self._calcIowait()
        # Detect iowait exception
        self._checkIOwaitException(iowait)

        # collect iostat end
        self.fDiskStats.seek(0)
        for stat in self.fDiskStats.readlines():
            stat = stat.split()
            if os.path.exists('/sys/block/'+stat[2]) == False or stat[2].startswith("loop"):
                if stat[2] in fieldDicts.keys():
                    self._removeDiskMonitor(stat[2])
                continue
            try:
                for idx, value in fieldDicts[stat[2]].items():
                    value[1] = long(stat[int(idx) + 2])
            except Exception:
                continue

        for devname, field in fieldDicts.items():
            io = self._calcIoIndex(devname, field, secs)
            if io['util'] >= self.cfg.getCfgItem('util'):
                # There is IO Burst at present, turn off threshold compensation
                self._disableThreshComp(devname, io['qusize'])
            # Detect util exception
            ioburst = self._checkUtilException(
                devname, io['util'], io['iops'], io['bps'], io['qusize'])
            # Detect await exception
            self._checkAwaitException(devname, io['wait'], ioburst)

        if ((self.uploadInter * secs) % 60) == 0:
            self._reportDataToRemote(fieldDicts.keys())


    def monitor(self):
        while True:
            secs = self.cfg.getCfgItem('cycle') / 1000.0
            self._collectBegin()
            time.sleep(secs)
            self._collectEnd(secs)
            # Check if it is necessary to start the diagnosis
            self.diagnose.checkDiagnose()
        self.fDiskStats.close()

