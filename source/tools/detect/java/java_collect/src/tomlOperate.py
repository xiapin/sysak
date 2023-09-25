# -*- coding: utf-8 -*-
"""
-------------------------------------------------
   File Name：     tomlOperate
   Description :
   Author :       liaozhaoyan
   date：          2023/6/14
-------------------------------------------------
   Change Activity:
                   2023/6/14:
-------------------------------------------------
"""
__author__ = 'liaozhaoyan'

import copy

import toml


class CtomlOperate(object):
    def __init__(self, model):
        super(CtomlOperate, self).__init__()
        with open(model, 'r') as f:
            self._model = toml.load(f)

    def out(self, d, fName):
        model = copy.deepcopy(self._model)
        model.update(d)
        with open(fName, 'w') as f:
            toml.dump(model, f)

    def confPid(self, pid, dua=5):
        fName = "conf_%d.toml" % pid
        d = {"destination": "/tmp/output_%d.fold" % pid}
        self.out(d, fName)
        return fName, None


if __name__ == "__main__":
    t = CtomlOperate("../continuous-profile-collector/configuration.toml")
    t.confPid(123)
    pass
