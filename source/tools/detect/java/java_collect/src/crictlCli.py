# -*- coding: utf-8 -*-
"""
-------------------------------------------------
   File Name：     crictlCli
   Description :
   Author :       liaozhaoyan
   date：          2023/7/28
-------------------------------------------------
   Change Activity:
                   2023/7/28:
-------------------------------------------------
"""
__author__ = 'liaozhaoyan'

from prun import Cprun
from dockerCli import CdockerCli


class CcrictlCli(CdockerCli):
    def __init__(self, cid, pid):
        super(CcrictlCli, self).__init__(cid, pid)
        self._engine = "crictl"

    def exec(self, cmd, tmo=1, ctmo=0.1):
        cmd = '%s exec %s sh -c \'%s\'' % (self._engine, self._cid, cmd)
        r = Cprun()
        return r.exec(cmd, tmo, ctmo)


if __name__ == "__main__":
    pass
