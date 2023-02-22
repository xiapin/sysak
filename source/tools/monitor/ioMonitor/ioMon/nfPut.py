# -*- coding: utf-8 -*-
"""
-------------------------------------------------
   File Name：     nfPut
   Description :
   Author :       liaozhaoyan
   date：          2022/4/28
-------------------------------------------------
   Change Activity:
                   2022/4/28:
-------------------------------------------------
"""
__author__ = 'liaozhaoyan'

import os
import socket
import requests
MAX_BUFF = 128 * 1024


class CnfPut(object):
    def __init__(self, pipeFile):
        self._sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
        self._path = pipeFile
        if not os.path.exists(self._path):
            raise ValueError("pipe path is not exist. please check unity is running.")


    def puts(self, s):
        if len(s) > MAX_BUFF:
            raise ValueError("message len %d, is too long ,should less than%d" % (len(s), MAX_BUFF))
        return self._sock.sendto(s, self._path)


if __name__ == "__main__":
    nf = CnfPut("/tmp/sysom")
    pass
