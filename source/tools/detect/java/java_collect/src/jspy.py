# -*- coding: utf-8 -*-
"""
-------------------------------------------------
   File Name：     jspy
   Description :
   Author :       liaozhaoyan
   date：          2023/6/15
-------------------------------------------------
   Change Activity:
                   2023/6/15:
-------------------------------------------------
"""
__author__ = 'liaozhaoyan'

import os
import sys
import zipfile
from forkRun import CforkRun
from conApi import CconApi
from tomlWrite import CtomlWrite
from unityCli import CunityCli
from ptop import Cptop
from prun import CprunThread
from queue import Queue
import logging
import uuid

SYSAK_BASE = os.getenv("SYSAK_WORK_PATH")
if SYSAK_BASE is None:
    SYSAK_BASE = "/usr/local/sysak/.sysak_components/"
SYSAK_PATH = os.path.join(SYSAK_BASE, "tools/jruntime/")
JRUNT_OUT  = "/var/sysak/jruntime/"
JRUNT_TOOL = "continuous-profile-collector-agent.jar"

depDirCheck = """
if [ ! -d "/var/sysak/jruntime" ]; then
  mkdir -p /var/sysak/jruntime
  chmod 777 /var/sysak/jruntime
fi
rm -rf /var/sysak/jruntime/*
"""


class Cjspy(object):
    def __init__(self, args):
        self._args = args
        os.system(depDirCheck)
        self._cwd = os.getcwd()
        # self._cli = CunityCli()
        self._cli = None
        self._capi = CconApi()
        logging.basicConfig(filename=os.path.join(JRUNT_OUT, "jspy.log"),
                            format='%(asctime)s %(levelname)s <%(module)s> [%(funcName)s] %(message)s',
                            datefmt='%Y-%m-%d,%H:%M:%S',
                            level=logging.DEBUG)
        os.chdir(SYSAK_PATH)

    def __del__(self):
        os.chdir(self._cwd)

    def _raptor(self, sample):
        overtime = sample + 1
        cmd = "./raptor oncpu --server local --exit-time %d --sample-rate 100 --upload-rate %ds" % (overtime, sample)
        print(cmd)
        logging.info("raptor cmd: %s" % cmd)
        oFile = os.path.join(JRUNT_OUT, "raptor.fold")
        return CforkRun(cmd, overtime + 5, oFile)

    def _pre_con(self, pid, confFile, outFile, cDict):
        if self._capi.isHost(pid):
            cli = None
        else:
            cli = self._capi.conCli(pid)
            self._capi.checkDir(cli, SYSAK_PATH, '755')
            self._capi.checkDir(cli, JRUNT_OUT, '777')
            jar_tar = os.path.join(SYSAK_PATH, JRUNT_TOOL)
            if not self._capi.checkFile(cli, jar_tar):
                self._capi.dCopyTo(cli, os.path.join(SYSAK_PATH, JRUNT_TOOL), JRUNT_TOOL)
            self._capi.dCopyTo(cli, os.path.join(SYSAK_PATH, confFile), confFile)

            if pid not in cDict:
                cDict[pid] = {"cp": [], "rm": []}
            oFile = os.path.join(JRUNT_OUT, outFile)
            cpcLog = os.path.join(JRUNT_OUT, "cpc_%d.log" % pid)
            cDict[pid]['cp'].append(oFile)
            cDict[pid]['cp'].append(cpcLog)
            cDict[pid]['rm'].append(os.path.join(SYSAK_PATH, confFile))
            cDict[pid]['rm'].append(oFile)
            cDict[pid]['rm'].append(cpcLog)
            cDict[pid]['rm'].append("/tmp/cpc-async-profiler-*.jfr")
        return cli

    def _pullConDatas(self, cDict):
        for pid, cell in cDict.items():
            cli = self._capi.conCli(pid)
            for cp in cell['cp']:
                self._capi.dCopyFrom(cli, os.path.join(JRUNT_OUT, cp), cp)
            for rm in cell['rm']:
                self._capi.exec(cli, "rm %s" % rm)
                
    def mon_pid(self, pid, conf, cDict, runs, q):
        confFile, outFile = conf.confPid(pid, self._args.duration)
        self._pre_con(pid, confFile, outFile, cDict)
        cmd = "/bin/sh collector -d %d %s -c %s" % (self._args.duration, pid, confFile)
        print(cmd)
        logging.info("java cmd: %s", cmd)
        runs.append(CprunThread(cmd, confFile, q, self._args.duration + 1))
        
    def mon_top(self, conf, cDict, runs, q):
        t = Cptop()
        tops = t.jtop(self._args.top)
        for top in tops:
            pid = top.pid
            self.mon_pid(pid, conf, cDict, runs, q)

    def z_dir(self, path, dDir):
        pwd = os.getcwd()
        os.chdir(dDir)
        with zipfile.ZipFile(path, mode='w', compression=zipfile.ZIP_DEFLATED) as zf:
            for root, dirs, files in os.walk("./"):
                for f in files:
                    zf.write(os.path.join(root, f))
        os.chdir(pwd)

    def diag(self):
        cDict = {}
        runs = []
        q = Queue()
        conf = CtomlWrite(JRUNT_OUT, "./configuration.toml")

        if self._args.top > 0:
            self.mon_top(conf, cDict, runs, q)
        else:
            s = self._args.pid
            pids = s.split(",")
            for pid in pids:
                self.mon_pid(int(pid), conf, cDict, runs, q)
        
        if self._args.bpf:
            runs.append(self._raptor(self._args.duration))
        for r in runs:
            r.join()

        self._pullConDatas(cDict)
        if self._args.oss:
            if self._cli is None:
                self._cli = CunityCli()
            self._cli.ossDir(str(uuid.uuid4()), JRUNT_OUT)
        else:
            path = os.path.join(JRUNT_OUT, "../j_out.zip")
            self.z_dir(path, JRUNT_OUT)


if __name__ == "__main__":
    num = 5
    if len(sys.argv) >= 2:
        num = int(sys.argv[1])
    s = Cjspy(num)
    s.diag()
    pass
