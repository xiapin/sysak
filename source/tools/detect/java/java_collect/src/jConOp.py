# -*- coding: utf-8 -*-
"""
-------------------------------------------------
   File Name：     jConOp
   Description :
   Author :       liaozhaoyan
   date：          2023/6/20
-------------------------------------------------
   Change Activity:
                   2023/6/20:
-------------------------------------------------
"""
__author__ = 'liaozhaoyan'

import os
import time
from conApi import CconApi
from tomlWrite import CtomlWrite


class CjConOp(object):
    def __init__(self, pid):
        self._api = CconApi()
        self._cid = self._api.conId(pid)
        self._cpid = int(self._con_pid(pid))

    def _con_pid(self, pid):
        with open("/proc/%d/status" % pid, 'r') as f:
            for _, line in enumerate(f):
                if line.startswith("NSpid:"):
                    _, _, cpid = line.split()
                    return cpid
        return None

    def exec(self, cmd, tmo=5, ctmo=0.1):
        return self._api.exec(self._cid, cmd, tmo, ctmo)

    def checkDir(self, path):
        ret = self.exec("ls -l %s" % path)
        if "cannot access" in ret:
            self.exec("mkdir -p %s" % path)

    def checkFlag(self, path):
        ret = self.exec("ls -l %s" % path)
        if "cannot access" in ret:
            return "None"
        ret = self.exec("cat %s" % path)
        if "ok" in ret:
            return "exist"
        return "locked"

    def _checkTool(self):
        dstExec = "/var/sysak/continuous-profile-collector/"
        self.checkDir(dstExec)
        ret = self.checkFlag(dstExec + "ok")
        if ret == "None":
            with open("ok", 'w') as f:
                f.write("lock")
            self._api.dCopyTo(self._cid, dstExec, "ok")
            self._api.dCopyTos(self._cid, dstExec, "../continuous-profile-collector/")
            with open("ok", 'w') as f:
                f.write("ok")
            self._api.dCopyTo(self._cid, dstExec, "ok")
        elif ret == "locked":
            cnt = 0
            while cnt < 50:
                ret = self.checkFlag(dstExec + "ok")
                if ret == "exist":
                    break
                cnt += 1
        self.checkDir("/var/sysak/jruntime")

    def jDiag(self):
        self._checkTool()
        cmds = ['sh', '-c', "cd /var/sysak/continuous-profile-collector/ && ls"]
        print(self.exec(cmds))
        conf = CtomlWrite("configuration.toml")
        confFile = conf.confPid(self._cpid)
        self._api.dCopyTo(self._cid, "/var/sysak/continuous-profile-collector/", confFile)
        cmd = "./collector -d %d %d -c %s" % (10, self._cpid, confFile)
        cmds = ['sh', '-c', "cd /var/sysak/continuous-profile-collector/ && %s" % cmd]
        print(self.exec(cmds))


if __name__ == "__main__":
    os.chdir("../continuous-profile-collector/")
    op = CjConOp(27966)
    op.jDiag()
