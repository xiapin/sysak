# -*- coding: utf-8 -*-
"""
-------------------------------------------------
   File Name：     prun
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
import select
import shlex
import time
import traceback
import logging
from subprocess import PIPE, Popen
from threading import Thread
import queue

ON_POSIX = 'posix' in sys.builtin_module_names


class Cprun(object):
    def __init__(self):
        super(Cprun, self).__init__()
        pass

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

    def _pollRead(self, p, tmo, ctmo=0.1):
        wait = self._pollTime(tmo, ctmo)
        s = ""
        with select.epoll() as poll:
            poll.register(p.stdout.fileno(), select.EPOLLIN)
            poll.register(p.stderr.fileno(), select.EPOLLIN)
            while True:
                events = poll.poll(wait())
                if len(events) == 0:    # poll time out.
                    return s
                for fd, event in events:
                    if event & select.EPOLLIN:
                        s += os.read(fd, 1024 * 1024).decode()
                    if event & (select.EPOLLHUP | select.EPOLLERR):
                        return s

    def exec(self, cmd, tmo=5, ctmo=0.1):
        # traceback.print_stack()
        # print("cwd", os.getcwd())
        p = Popen(shlex.split(cmd),
                  stdout=PIPE,
                  stderr=PIPE,
                  stdin=PIPE,
                  close_fds=ON_POSIX)
        ret = self._pollRead(p, tmo, ctmo)
        p.terminate()
        return ret


class CprunThread(Thread):
    def __init__(self, cmd, toDel, q, tmo=5):
        self._cmd = cmd
        self._toDel = toDel
        self._tmo = tmo
        self._q = q
        super(CprunThread, self).__init__()
        self.daemon = True
        self.start()

    def run(self):
        r = Cprun()
        self._q.put(r.exec(self._cmd, self._tmo, self._tmo))
        logging.info("exec: %s done" % self._cmd)
        if len(self._toDel):
            os.remove(self._toDel)


if __name__ == "__main__":
    q = queue.Queue()
    t = CprunThread("ps", q, "")
    t.join()
    print(q.get(block=False))
    pass
