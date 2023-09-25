# -*- coding: utf-8 -*-
"""
-------------------------------------------------
   File Name：     dockerCli
   Description :
   Author :       liaozhaoyan
   date：          2023/6/27
-------------------------------------------------
   Change Activity:
                   2023/6/27:
-------------------------------------------------
"""
__author__ = 'liaozhaoyan'

import os
import random
import logging
from nsenter import Namespace
from prun import Cprun
from multiprocessing import Process


class CprocCopyTo(Process):
    def __init__(self, pid, dst, src):
        super(CprocCopyTo, self).__init__()
        self._pid = pid
        self._dst = dst
        self._src = src
        self.start()
        self.join()

    def run(self):
        with open(self._src, 'rb') as f:
            stream = f.read()
        with Namespace(self._pid, "mnt"):
            with open(self._dst, 'wb') as f:
                f.write(stream)


class CprocCopyFrom(Process):
    def __init__(self, pid, dst, src):
        super(CprocCopyFrom, self).__init__()
        self._pid = pid
        self._dst = dst
        self._src = src
        self.start()
        self.join()

    def run(self):
        cwd = os.getcwd()
        with Namespace(self._pid, "mnt"):
            try:
                with open(self._src, 'rb') as f:
                    stream = f.read()
            except FileNotFoundError:
                os.chdir(cwd)
                logging.warning("con cp from, no: %s" % self._src)
                return

        os.chdir(cwd)
        with open(self._dst, 'wb') as f:
            f.write(stream)


class CdockerCli(object):
    def __init__(self, cid, pid):
        self._cid = cid
        self._pid = pid
        self._engine = "docker"

    def cid(self):
        return self._cid

    def exec(self, cmd, tmo=1, ctmo=0.1):
        cmd = '%s exec --user=root %s sh -c \'%s\'' % (self._engine, self._cid, cmd)
        r = Cprun()
        return r.exec(cmd, tmo, ctmo)

    def copyTo(self, dst, src):
        CprocCopyTo(self._pid, dst, src)

    def copyFrom(self, dst, src):
        CprocCopyFrom(self._pid, dst, src)

    def list(self):
        cmd = "%s ps" % self._engine
        r = Cprun()
        return r.exec(cmd)


if __name__ == "__main__":
    cli = CdockerCli("43818bdc1883")
    print(cli.exec("ps"))
    pass
