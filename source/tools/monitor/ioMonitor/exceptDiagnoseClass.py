# -*- coding: utf-8 -*-

import os
import string
import time
from collections import OrderedDict
import threading
from tools.iofstool.iofsstat import iofsstatStart
from tools.iowaitstat.iowaitstat import iowaitstatStart
from displayClass import displayClass


class runDiag(object):
    def __init__(self, logRootPath, sender):
        self.funcDicts = {
            'iohang': self.startIohangDiagnose,
            'ioutil': self.startIoutilDiagnose,
            'iolatency': self.startIolatencyDiagnose,
            'iowait': self.startIowaitDiagnose}
        self.lastDiagTimeDicts = \
            {'iohang': 0, 'ioutil': 0, 'iolatency': 0, 'iowait': 0}
        self.display = displayClass(sender)
        self.sysakPath = 'sysak'
        self.logRootPath = logRootPath


    def _recentDiagnoseValid(self, diagType):
        return self.display.diagnoseValid(diagType)


    def startIohangDiagnose(self, *argv):
        devname = argv[0]
        now = time.time()
        if now - self.lastDiagTimeDicts['iohang'] <= 60:
            return
        startTime = time.strftime('%Y-%m-%d-%H:%M:%S', time.localtime(now))
        logdir = self.logRootPath+'/iosdiag/hangdetect/'+startTime
        outlog = logdir+'/resultCons.log'
        if not os.path.exists(logdir):
            try:
                os.makedirs(logdir)
            except Exception:
                return
        self.lastDiagTimeDicts['iohang'] = now
        if devname is not None:
            os.system(self.sysakPath+' -g iosdiag hangdetect -o -t 3000 -T 10 -f '+
                      logdir+' '+devname+' > '+outlog+' &')
        else:
            os.system(self.sysakPath+' -g iosdiag hangdetect -o -t 3000 -T 10 -f '+
                      logdir+' > '+outlog+' &')
        self.display.start(20, 'iohang', logdir, now, now+60)


    def startIolatencyDiagnose(self, *argv):
        devname = argv[0]
        thresh = argv[1]
        # ioburst = argv[2]
        now = time.time()
        if now - self.lastDiagTimeDicts['iolatency'] <= 60:
            return
        startTime = time.strftime('%Y-%m-%d-%H:%M:%S', time.localtime(now))
        logdir = self.logRootPath+'/iosdiag/latency/'+startTime
        outlog = logdir+'/resultCons.log'
        if not os.path.exists(logdir):
            try:
                os.makedirs(logdir)
            except Exception:
                return
        self.lastDiagTimeDicts['iolatency'] = now
        if devname is not None:   
            os.system(self.sysakPath+' -g iosdiag latency -t ' + str(thresh) +
                    ' -T 30 -m -f '+logdir+' '+devname+' > '+outlog+' &')
        else:
            os.system(self.sysakPath+' -g iosdiag latency -t ' + str(thresh) +
                      ' -T 30 -m -f '+logdir+' > '+outlog+' &')
        # if ioburst:
        #     self.display.markIoburst(now)
        # self.display.start(60, 'iolatency', logdir, now, now+60)


    def startIoutilDiagnose(self, *argv):
        devname = argv[0]
        bwThresh = argv[1]
        iopsThresh = argv[2]
        now = time.time()
        if now - self.lastDiagTimeDicts['ioutil'] <= 60:
            return
        startTime = time.strftime('%Y-%m-%d-%H:%M:%S', time.localtime(now))
        logdir = self.logRootPath+'/iosdiag/iofsstat/'+startTime
        outlog = logdir+'/resultCons.log'
        if not os.path.exists(logdir):
            try:
                os.makedirs(logdir)
            except Exception:
                return
        self.lastDiagTimeDicts['ioutil'] = now
        #self.display.setIoburstThresh(iopsThresh, bwThresh)
        argvs = ['-j',outlog,'-n','-m','-c','1','-t','5','-T','40',
            '-i',str(iopsThresh),'-b',str(bwThresh)]
        threading.Thread(target=iofsstatStart, args=(argvs,)).start()
        self.display.start(55, 'ioutil', outlog, now, now+60)


    def startIowaitDiagnose(self, *argv):
        iowaitThresh = argv[0]
        now = time.time()
        if now - self.lastDiagTimeDicts['iowait'] <= 60:
            return
        startTime = time.strftime('%Y-%m-%d-%H:%M:%S', time.localtime(now))
        logdir = self.logRootPath+'/iosdiag/iowaitstat/'+startTime
        outlog = logdir+'/resultCons.log'
        if not os.path.exists(logdir):
            try:
                os.makedirs(logdir)
            except Exception:
                return
        self.lastDiagTimeDicts['iowait'] = now
        argvs = ['-j', outlog, '-t', '5', '-w', str(iowaitThresh), '-T', '45']
        threading.Thread(target=iowaitstatStart, args=(argvs,)).start()
        self.display.start(55, 'iowait', outlog, now, now+60)


    def runDiagnose(self, diagType, argv):
        self.funcDicts[diagType](*list(argv))


class diagnoseClass(runDiag):
    def __init__(self, window, logRootPath, sender):
        super(diagnoseClass, self).__init__(logRootPath, sender)
        self.window = window
        self.diagnoseDicts = OrderedDict()
        self._diagStat = OrderedDict(
                {'iohang': {'run': False, 'argv': [0, 0, 0, 0, 0, 0, 0, 0]},
                'ioutil': {'run': False, 'argv': [0, 0, 0, 0, 0, 0, 0, 0]},
                'iowait': {'run': False, 'argv': [0, 0, 0, 0, 0, 0, 0, 0]},
                'iolatency': {'run': False, 'argv': [0, 0, 0, 0, 0, 0, 0, 0]}})


    def addItem(self, devname, key, reportInterval, triggerInterval):
        diagRecord = {
            'statWindow': self.window,
            'trigger': False,
            'lastReport': 0,
            'reportInterval': reportInterval,
            'reportCnt': 0,
            'lastDiag': 0,
            'triggerInterval': triggerInterval,
            'diagArgs': [0, 0, 0, 0, 0, 0, 0, 0]}
        if devname not in self.diagnoseDicts.keys():
            self.diagnoseDicts.setdefault(devname, {key: diagRecord})
        else:
            self.diagnoseDicts[devname].setdefault(key, diagRecord)


    def setUpDiagnose(self, devname, key, nrSample, *argv):
        diagnoseDicts = self.diagnoseDicts[devname][key]
        lastDiag = diagnoseDicts['lastDiag']
        lastReport = diagnoseDicts['lastReport']
        statWindow = diagnoseDicts['statWindow']
        reportInterval = diagnoseDicts['reportInterval']
        triggerInterval = diagnoseDicts['triggerInterval']

        if reportInterval != 0:
            if lastReport == 0 or (nrSample-lastReport) > statWindow:
                diagnoseDicts['lastReport'] = nrSample
                diagnoseDicts['reportCnt'] = 1
            else:
                diagnoseDicts['reportCnt'] += 1
            if diagnoseDicts['reportCnt'] > reportInterval:
                if lastDiag == 0 or (nrSample-lastDiag) > triggerInterval:
                    diagnoseDicts['trigger'] = True
                    diagnoseDicts['reportCnt'] = 0
                    diagnoseDicts['lastDiag'] = nrSample
                else:
                    diagnoseDicts['lastReport'] = nrSample
                    diagnoseDicts['reportCnt'] = 0
        elif triggerInterval != 0:
            if lastDiag == 0 or (nrSample-lastDiag) >= triggerInterval:
                diagnoseDicts['lastDiag'] = nrSample
                diagnoseDicts['trigger'] = True
        else:
            diagnoseDicts['trigger'] = True

        for idx, val in enumerate(argv):
            diagnoseDicts['diagArgs'][idx] = val


    def isException(self, devname, key):
        diagnoseDicts = self.diagnoseDicts[devname][key]
        reportInterval = diagnoseDicts['reportInterval']
        triggerInterval = diagnoseDicts['triggerInterval']

        if reportInterval != 0:
            if (diagnoseDicts['reportCnt'] + 1) >= reportInterval:
                return True
        elif triggerInterval != 0:
            return True
        else:
            return True
        return False


    def clearDiagStat(self):
        for diagType, stat in self._diagStat.items():
            stat['run'] = False
            stat['argv'][0:] = [0, 0, 0, 0, 0, 0, 0, 0]


    def checkDiagnose(self):
        diagnoseDicts = self.diagnoseDicts
        diagInfo = {'iohang': [], 'iolatency': [], 'ioutil': []}
        diagStat = self._diagStat
        ioburst = False

        for devname, diagDict in diagnoseDicts.items():
            if devname == 'system':
                if diagDict['iowait']['trigger'] == True:
                    diagStat['iowait']['run'] = True
                    diagStat['iowait']['argv'][0] = \
                        diagDict['iowait']['diagArgs'][0]
                    diagDict['iowait']['trigger'] = False
                continue

            for diagType in ['iohang', 'iolatency', 'ioutil']:
                if diagDict[diagType]['trigger'] == True:
                    if diagType == 'iolatency':
                        ioburst = diagDict['iolatency']['diagArgs'][1]
                    diagInfo[diagType].append(devname)
                    diagDict[diagType]['trigger'] = False

        for diagType, value in diagInfo.items():
            diagStat[diagType]['run'] = True
            if len(value) > 1:
                diagStat[diagType]['argv'][0] = None
                # max_threshold = diagnoseDicts[value[0]]['iolatency']['diagArgs'][0]
                # for dev in value:
                #     if max_threshold <= diagnoseDicts[dev]['iolatency']['diagArgs'][0]:
                #         diagStat[diagType]['argv'][0] = dev
                #         max_threshold = diagnoseDicts[dev]['iolatency']['diagArgs'][0]
            elif len(value) == 1:
                diagStat[diagType]['argv'][0] = None
                # diagStat[diagType]['argv'][0] = value[0]
            else:
                diagStat[diagType]['run'] = False

        if diagStat['ioutil']['run'] == True:
            for idx in [1,2]:
                val = sorted(
                    [diagnoseDicts[dev]['ioutil']['diagArgs'][idx-1] 
                    for dev in diagInfo['ioutil']],
                    reverse=True)
                diagStat['ioutil']['argv'][idx] = val[-1]

        if diagStat['iolatency']['run'] == True:
            # print("diagStat['iolatency']:", diagStat['iolatency'])
            diagStat['iolatency']['argv'][1] = sorted(
                [diagnoseDicts[dev]['iolatency']['diagArgs'][0] 
                for dev in diagInfo['iolatency']],
                reverse=True)[-1]
            diagStat['iolatency']['argv'][2] = ioburst

        for diagType, stat in diagStat.items():
            if stat['run'] == True:
                self.runDiagnose(diagType, stat['argv'])
                stat['run'] = False


    # In displayClass, after the diagnostic log is reported to the remote end, 
    # it will be marked as a valid diagnosis, and In exceptDiagnoseClass, 
    # clear the valid mark before each diagnosis
    def recentDiagnoseValid(self, diagType):
        return self._recentDiagnoseValid(diagType)

