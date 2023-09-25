#!/usr/bin/python
# -*- coding: utf-8 -*-

import os
import sys
import signal
import string
import time
import json
from collections import OrderedDict

globalCfgDicts = {}
globalCfgPath = ''
def loadCfg(cfgPath):
    with open(cfgPath) as f:
        data = f.read()
    return json.loads(data)


def loadCfgHander(signum, frame):
    global globalCfgDicts
    globalCfgDicts = loadCfg(globalCfgPath)


class ioMonCfgClass(object):
    def __init__(self, cfgArg, resetCfg, logRootPath):
        global globalCfgPath
        self.cfgPath = logRootPath+'/ioMon/ioMonCfg.json'
        globalCfgPath = self.cfgPath
        cfg = self._paserCfg(cfgArg)
        hasArgs = any(list(cfg.values()))
        if not os.path.exists(self.cfgPath) or resetCfg:
            cfg['iowait'] = int(cfg['iowait']) if cfg['iowait'] else 5
            # cfg['await'] = int(cfg['await']) if cfg['await'] else 2
            cfg['await'] = 2
            cfg['util'] = int(cfg['util']) if cfg['util'] else 20
            cfg['iops'] = int(cfg['iops']) if cfg['iops'] else 150
            cfg['bps'] = int(cfg['bps']) if cfg['bps'] else 31457280
            cfg['cycle'] = int(cfg['cycle']) if cfg['cycle'] else 1000
            cfg['diagIowait'] = cfg['diagIowait'] if cfg['diagIowait'] else 'off'
            cfg['diagIoburst'] = cfg['diagIoburst'] if cfg['diagIoburst'] else 'off'
            # cfg['diagIolat'] = cfg['diagIolat'] if cfg['diagIolat'] else 'off'
            cfg['diagIolat'] = 'on'
            cfg['diagIohang'] = cfg['diagIohang'] if cfg['diagIohang'] else 'off'
            self._updateCfg(cfg)
            return
        else:
            self._loadCfg()
        if hasArgs:
            self._updateCfg(cfg)


    def _paserCfg(self, cfgArg):
        cfgDicts = {
            'iowait':None, 'await':None, 'util':None, 'iops':None, 'bps':None,
            'cycle':None, 'diagIowait':None, 'diagIoburst':None,
            'diagIolat':None, 'diagIohang':None}
        try:
            cfgList = \
                cfgArg.split(',') if cfgArg is not None and len(cfgArg) > 0 else []
            for cfg in cfgList:
                errstr = None
                c = cfg.split('=')
                if c[0] not in cfgDicts.keys() or len(c[1]) == 0:
                    errstr = "bad cfg item: %s, must be in %s" %(
                        cfg, str(cfgDicts.keys()))
                elif 'diag' not in c[0] and not c[1].isdigit():
                    errstr = "monitor cfg argv must be digit: %s." %cfg
                elif 'diag' in c[0] and c[1] not in ['on', 'off']:
                    errstr = \
                        "diagnose cfg argv must be [\'on\', \'off\']: %s." %cfg
                if errstr:
                    print(errstr)
                    sys.exit(0)
                cfgDicts[c[0]] = c[1]
        except Exception:
            print("bad cfg: %s." %cfg)
            sys.exit(0)
        return cfgDicts


    def _setGlobalCfgDicts(self, CfgDicts):
        global globalCfgDicts
        globalCfgDicts = CfgDicts


    def _getGlobalCfgDicts(self):
        global globalCfgDicts
        return globalCfgDicts


    def _updateCfg(self, cfgDicts):
        oldCfg = {}
        if not os.path.exists(self.cfgPath):
            if not os.path.exists(os.path.dirname(self.cfgPath)):
                os.mkdir(os.path.dirname(self.cfgPath))
        else:
            oldCfg = loadCfg(self.cfgPath)
        f = open(self.cfgPath, 'w+')
        newCfg = json.loads(json.dumps(cfgDicts))
        if oldCfg:
            for key,val in newCfg.items():
                if val is not None:
                    oldCfg[key] = val
            newCfg = oldCfg
        s = json.dumps(newCfg, indent=4)
        f.write(s)
        f.close()
        self._setGlobalCfgDicts(newCfg)


    def _loadCfg(self):
        self._setGlobalCfgDicts(loadCfg(self.cfgPath))


    def createCfgFlagFile(self):
        f = open(os.path.dirname(self.cfgPath)+'/.ioMonCfgFlag', 'w+')
        f.write(str(os.getpid()))
        f.close()
        signal.signal(signal.SIGUSR2, loadCfgHander)


    def notifyIoMon(self):
        try:
            with open(os.path.dirname(self.cfgPath)+'/.ioMonCfgFlag') as f:
                pid = f.read()
            with open('/proc/'+str(pid)+'/cmdline') as f:
                cmdline = f.read().strip()
        except Exception:
            sys.exit(0)
        if 'ioMonitor' in cmdline:
            os.system('kill -USR2 '+str(pid))


    def getCfgItem(self, key):
        val = str(self._getGlobalCfgDicts()[key])
        if val.isdigit():
            val = int(val)
        return val
