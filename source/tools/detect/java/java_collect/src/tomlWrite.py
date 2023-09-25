# -*- coding: utf-8 -*-
"""
-------------------------------------------------
   File Name：     tomlWrite
   Description :
   Author :       liaozhaoyan
   date：          2023/6/16
-------------------------------------------------
   Change Activity:
                   2023/6/16:
-------------------------------------------------
"""
__author__ = 'liaozhaoyan'

import re


class CtomlWrite(object):
    def __init__(self, jout, model):
        super(CtomlWrite, self).__init__()
        self._out = jout
        self._model = model
        self._reId = re.compile(r"([0-9a-f]){16,}")

    def _cgroup(self, pid):
        d = {}
        with open("/proc/%d/cgroup" % pid, 'r') as f:
            for i, line in enumerate(f):
                n, t, path = line.strip().split(":")
                d[t] = path
        return d

    def outName(self, pid):
        cgroups = self._cgroup(pid)
        cname = cgroups['name=systemd']
        ret = self._reId.search(cname)
        if ret:
            cid = ret[0][:8]
            return "out_%d_%s.jfr" % (pid, cid)
        return "out_%d.jfr" % pid
            
    def confPid(self, pid, dua=5):
        fName = "conf_%d.toml" % pid
        outName = self.outName(pid)
        with open(self._model, 'r') as f:
            s = f.read()
        rOrig = "log_file = \\'\\/tmp\\/cpc_\\?\\.log\\'"
        rDur = "interval = -1"
        sRep = "log_file = '%scpc_%d.log'" % (self._out, pid)
        sDur = "interval = %d" % dua
        s = re.sub(rOrig, sRep, s)
        s = re.sub(rDur, sDur, s)
        with open(fName, 'w') as f:
            stream = "\n".join([s, "destination = '%s%s'" % (self._out, outName)])
            f.write(stream)
        return fName, outName


if __name__ == "__main__":
    t = CtomlWrite("/var/sysak/jruntime/", "../continuous-profile-collector/configuration.toml")
    t.confPid(123)
    pass
