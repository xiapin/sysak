#!/usr/bin/python
# -*- coding: utf-8 -*-

import os
import sys
import string
import re
from subprocess import PIPE, Popen


def execCmd(cmd):
    p = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
    return p.stdout.read().decode('utf-8')


def echoFile(filename, txt):
    execCmd("echo \'"+txt+"\' > "+filename)


def echoFileAppend(filename, txt):
    execCmd("echo \'"+txt+"\' >> "+filename)


def humConvert(value, withUnit):
    units = ["B", "KB", "MB", "GB", "TB", "PB"]
    size = 1024.0

    if value == 0:
        return value

    for i in range(len(units)):
        if (value / size) < 1:
            if withUnit:
                return "%.1f%s/s" % (value, units[i])
            else:
                return "%.1f" % (value)
        value = value / size


def getDevt(devname):
    try:
        with open('/sys/class/block/' + devname + '/dev') as f:
            dev = f.read().split(':')
            return ((int(dev[0]) << 20) + int(dev[1]))
    except Exception:
        return -1


def getDevtRegion(devname):
    if os.path.exists('/sys/block/'+devname):
        isPart = False
    elif os.path.exists('/sys/class/block/'+devname):
        isPart = True
    else:
        return [-1, -1]

    master = devname if not isPart else \
        os.readlink('/sys/class/block/'+devname).split('/')[-2]
    partList = list(
        filter(lambda x: master in x,
        os.listdir('/sys/class/block/'+master)))
    if not partList:
        partList = []
    partList.append(master)
    return [getDevt(p) for p in partList]


def getTgid(pid):
    try:
        with open("/proc/"+str(pid)+"/status") as f:
            return ''.join(re.findall(r'Tgid:(.*)', f.read())).lstrip()
    except IOError:
        return '-'
    return '-'


def fixComm(comm, pid):
    try:
        if ".." in comm:
            with open("/proc/"+str(pid)+"/comm") as f:
                return f.read().rstrip('\n')
    except IOError:
        return comm
    return comm


def getContainerId(pid):
    try:
        piddir = "/proc/"+str(pid)
        with open(piddir+"/cgroup") as f:
            # ...
            # cpuset,cpu,cpuacct:/docker/e2afa607d8f13e5b1f89d38ee86d86....
            # memory:/docker/e2afa607d8f13e5b1f89d38ee86.....
            # ...
            cid = f.read().split('memory:')[1].split('\n')[0].rsplit('/',1)[1]
            if not cid:
                cid = '-'
    except Exception:
        cid = '-'
    return cid


def getFullNameFromProcPid(pid, ino):
    try:
        piddir = "/proc/"+str(pid)
        # list the open files of the task
        fdList = os.listdir(piddir+"/fd")
        for f in fdList:
            try:
                path = os.readlink(piddir+"/fd/"+f)
                if '/dev/' in path or '/proc/' in path or '/sys/' in path:
                    continue

                if os.path.isfile(path) and os.stat(path).st_ino == int(ino):
                    return path
            except (IOError, EOFError) as e:
                continue
    except Exception:
        pass
    return "-"


def supportKprobe(name):
    file = '/sys/kernel/debug/tracing/available_filter_functions'
    with open(file) as f:
        ss = f.read()
    if ss.find(name) > 0:
        return True
    return False
