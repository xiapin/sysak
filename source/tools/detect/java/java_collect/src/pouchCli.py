# -*- coding: utf-8 -*-
"""
-------------------------------------------------
   File Name：     pouchCli
   Description :
   Author :       liaozhaoyan
   date：          2023/6/27
-------------------------------------------------
   Change Activity:
                   2023/6/27:
-------------------------------------------------
"""
__author__ = 'liaozhaoyan'

from dockerCli import CdockerCli


class CpouchCli(CdockerCli):
    def __init__(self, cid, pid):
        super(CpouchCli, self).__init__(cid, pid)
        self._engine = "pouch"


if __name__ == "__main__":
    pass
