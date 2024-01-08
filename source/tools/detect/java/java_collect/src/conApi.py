# -*- coding: utf-8 -*-
"""
-------------------------------------------------
   File Name：     conApi
   Description :
   Author :       liaozhaoyan
   date：          2023/6/12
-------------------------------------------------
   Change Activity:
                   2023/6/12:
-------------------------------------------------
"""
__author__ = 'liaozhaoyan'

import os
import re
from dockerCli import CdockerCli
from pouchCli import CpouchCli
from crictlCli import CcrictlCli


class CconApi(object):
    def __init__(self):
        super(CconApi, self).__init__()
        self._reNs = re.compile(r"(?<=\[)(.+?)(?=\])")
        self._reDocker = re.compile(r"([0-9a-f]){16,}")
        self._rePod = re.compile(r"\/.*\/([0-9a-f]){16,}")
        self._ns1 = self._ns_mnt(1)

    def _ns_mnt(self, pid):
        try:
            link = os.readlink("/proc/%d/ns/mnt" % pid)
            return self._reNs.findall(link)[0]
        except FileNotFoundError:
            return None
        except IndexError:
            return None

    def isHost(self, pid):
        return self._ns_mnt(pid) == self._ns1

    def _cgroup(self, pid):
        d = {}
        with open("/proc/%d/cgroup" % pid, 'r') as f:
            for i, line in enumerate(f):
                n, t, path = line.strip().split(":")
                d[t] = path
        return d

    def conCli(self, pid):
        cgroups = self._cgroup(pid)
        cname = cgroups['name=systemd']
        if "/docker" in cname:
            ret = self._reDocker.search(cname)[0]
            if ret:
                return CdockerCli(ret, pid)
        elif self._rePod.search(cname):
            podId = cname.split("/")[-1]
            return CpouchCli(podId, pid)
        elif "/cri-containerd" in cname:
            s = cname.split("-")[-1]
            podId = s.split(".")[0]
            return CcrictlCli(podId, pid)
        return None

    def dCopyTo(self, cli, dst, src):
        cli.copyTo(dst, src)

    def dCopyFrom(self, cli, dst, src):
        cli.copyFrom(dst, src)

    @staticmethod
    def _pollTime(tmo, ctmo):
        first = True
        def waitTime():
            nonlocal first
            if first:
                first = False
                return tmo
            else:
                return ctmo
        return waitTime

    def exec(self, cli, cmd, tmo=5, ctmo=0.1):
        return cli.exec(cmd, tmo, ctmo)

    def checkDir(self, cli, path, mode):
        ret = cli.exec("ls -l %s" % path)
        if "cannot access" in ret:
            cli.exec("mkdir -p %s && chmod %s %s" % (path, mode, path))

    def checkFile(self, cli, path):
        ret = cli.exec("ls -l %s" % path)
        if ret == "" or "cannot access" in ret:
            return False
        else:
            return True

    def conList(self, cli):
        return cli.list()


if __name__ == "__main__":
    api = CconApi()
    cli = api.conCli(355)
    res = api.exec(cli, "ls -l /var/sysak/jruntime")
    if "cannot access" in res:
        api.exec(cli, "mkdir -p /var/sysak/jruntime")
    pass
