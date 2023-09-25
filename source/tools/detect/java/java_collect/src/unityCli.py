# -*- coding: utf-8 -*-
"""
-------------------------------------------------
   File Name：     unityQuery
   Description :
   Author :       liaozhaoyan
   date：          2023/5/10
-------------------------------------------------
   Change Activity:
                   2023/5/10:
-------------------------------------------------
"""
__author__ = 'liaozhaoyan'

import os
import sys
import requests
import json
import zipfile
import tempfile
import base64
import uuid
import yaml


class CunityCli(object):
    def __init__(self, fYaml="/etc/sysak/base.yaml"):
        super(CunityCli, self).__init__()
        with open(fYaml, 'r') as f:
            data = yaml.load(f, Loader=yaml.FullLoader)
        self._url = "http://127.0.0.1:%d/api/" % data["config"]["port"]

    def query(self, tab, ts="1m"):
        d = {"mode": "last", "time": ts, "table": [tab]}
        url = self._url + "query"
        res = requests.post(url, json=d)
        return res.content.decode()

    def line(self, lines):
        url = self._url + "line"
        res = requests.post(url, data=lines)
        return res.content.decode()

    def pushFile(self, title, fileName):
        with tempfile.TemporaryDirectory() as tmpdir:
            with zipfile.ZipFile(tmpdir + '/push.zip', 'w') as z:
                z.write(fileName)
            with open(tmpdir + '/push.zip', 'rb') as z:
                bs = z.read()
        stream = base64.b64encode(bs)
        lines = '%s content="%s"' % (title, stream.decode())
        return self.line(lines)

    def _ossPut(self, uid, stream):
        url = self._url + 'oss'
        headers = {'Content-Type': 'application/octet-stream', 'uuid': uid}
        res = requests.post(url, headers=headers, data=stream)
        return res.content.decode()

    def ossFile(self, uid, fName):
        with open(fName, 'rb') as f:
            stream = f.read()
            return self._ossPut(uid, stream)

    @staticmethod
    def zipDir(dDir):
        with tempfile.TemporaryFile() as tmp:
            with zipfile.ZipFile(tmp, mode='w', compression=zipfile.ZIP_DEFLATED) as zf:
                for root, dirs, files in os.walk(dDir):
                    for f in files:
                        zf.write(os.path.join(root, f))

            tmp.seek(0)
            stream = tmp.read()
            return stream

    def ossDir(self, uid, dDir):
        stream = self.zipDir(dDir)
        print(len(stream))
        return self._ossPut(uid, stream)


if __name__ == "__main__":
    q = CunityCli()
    print(q.ossDir(str(uuid.uuid4()), '../test'))
    pass
