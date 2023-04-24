# -*- coding: utf-8 -*-
"""
-------------------------------------------------
   File Name：     fileSync.py
   Description :
   Author :       liaozhaoyan
   date：          2023/4/18
-------------------------------------------------
   Change Activity:
                   2023/4/18:
-------------------------------------------------
"""
import os
import sys


FILE_SYNC = 0x010000
FILE_DIRECT = 0x040000


def getFlags(fdPath):
    try:
        with open(fdPath) as f:
            for i, line in enumerate(f):
                if line.startswith("flags:"):
                    _, ret = line.split(":", 1)
                    return ret.strip()
    except IOError:
        return 0


def getFdName(pid, fd):
    fdPath = "/proc/%s/fd/%s" % (pid, fd)
    return os.readlink(fdPath)


def getCmd(pid):
    path = "/proc/%s/cmdline" % pid
    try:
        with open(path) as f:
            return f.read()
    except IOError:
        return "nil"


def checkFile(fileLink):
    if fileLink.startswith("/") and not fileLink.startswith("/dev"):
        return True
    return False


def checkFlag(pid, fd, flag):
    vFlag = int(flag)
    link = getFdName(pid, fd)
    if checkFile(link):
        if vFlag & FILE_SYNC:
            print("pid:%s, cmd:%s, file:%s, set sync flag" % (pid, getCmd(pid), link))
        if vFlag & FILE_DIRECT:
            print("pid:%s, cmd:%s, file:%s, set direct flag" % (pid, getCmd(pid), link))


def checkPid(pid):
    path = "/proc/" + pid + "/fdinfo"
    for fd in os.listdir(path):
        fdPath = "/".join([path, fd])
        try:
            flag = getFlags(fdPath)
        except IOError:
            continue
        checkFlag(pid, fd, flag)


def walkGroup(path):
    path += "/tasks"
    try:
        with open(path) as f:
            for i, line in enumerate(f):
                checkPid(line.strip())
    except IOError:
        print("bad path.")


if __name__ == "__main__":
    path = "/sys/fs/cgroup/pids/kubepods/besteffort/slot_976/231a6b41a7be49fdcecac835bc1487272ba8319b8106692d2134c34b025931fa"
    if len(sys.argv) > 1:
        path = sys.argv[1]
    walkGroup(path)
