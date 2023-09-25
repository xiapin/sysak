# -*- coding: utf-8 -*-
"""
-------------------------------------------------
   File Name：     forkRun
   Description :
   Author :       liaozhaoyan
   date：          2023/6/8
-------------------------------------------------
   Change Activity:
                   2023/6/8:
-------------------------------------------------
"""
__author__ = 'liaozhaoyan'

import os
import time
import shlex
import psutil
from subprocess import PIPE, Popen
from threading import Thread
import logging


class Cguard(Thread):
    def __init__(self, pid, wait, cmd):
        super(Cguard, self).__init__()
        self.daemon = True
        self._pid = pid
        self._wait = wait
        self._cmd = cmd
        self.start()

    def run(self):
        time.sleep(self._wait)
        try:
            p = psutil.Process(self._pid)
        except psutil.NoSuchProcess:
            return
        if p and p.parent().pid == os.getpid():
            logging.warning("guard pid: %d cmd: %s over time." % (self._pid, self._cmd))
            os.kill(self._pid, 9)


class CforkRun(Thread):
    def __init__(self, cmd, wait, out_f):
        super(CforkRun, self).__init__()
        self._cmd = cmd
        self._wait = wait
        self._out = out_f

        self._pid = -1
        self.start()

    def run(self):
        with open(self._out, 'w') as f:
            p = Popen(shlex.split(self._cmd), stdout=f)
            if p:
                Cguard(p.pid, self._wait, self._cmd)
                p.wait()


if __name__ == "__main__":
    cmd = "./raptor oncpu --server local --exit-time 11 --sample-rate 100 --upload-rate 10s"
    t = CforkRun(cmd, 12, "out.fold")
    t.join()
    pass
