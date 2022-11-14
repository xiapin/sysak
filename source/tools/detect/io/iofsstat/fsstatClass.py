#!/usr/bin/python
# -*- coding: utf-8 -*-

import os
import sys
import signal
import string
import time
import re
import json
from collections import OrderedDict
from diskstatClass import diskstatClass
from diskstatClass import getDevt,getDevtRegion
from diskstatClass import humConvert
from subprocess import PIPE, Popen
from mmap import PAGESIZE


def execCmd(cmd):
    p = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
    return p.stdout.read().decode('utf-8')


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


def getFullNameFromProcPid(pid, ino):
    try:
        piddir = "/proc/"+str(pid)
        dockerRootPath = ''
        with open(piddir+"/cgroup") as f:
            # ...
            # cpuset,cpu,cpuacct:/docker/e2afa607d8f13e5b1f89d38ee86d86....
            # memory:/docker/e2afa607d8f13e5b1f89d38ee86.....
            # ...
            cid = f.read().split("docker/")
            # cid = e2afa607d8f1
            cid = cid[1][0:12] if len(cid) > 1 and len(cid[1]) > 12 else ""
            if re.match('\A[0-9a-fA-F]+\Z', cid):
                dockerRootPath = \
                    execCmd("docker inspect -f '{{.HostRootPath}}' "+cid).strip('\n')
        # list the open files of the task
        fdList = os.listdir(piddir+"/fd")
        for f in fdList:
            try:
                path = os.readlink(piddir+"/fd/"+f)
                if '/dev/' in path or '/proc/' in path or '/sys/' in path:
                    continue

                if os.path.isfile(path) and os.stat(path).st_ino == int(ino):
                    if len(dockerRootPath) > 0:
                        return path+"[containterId:%s]" % cid
                    return path

                if dockerRootPath != '':
                    dockerFullPath = dockerRootPath+path
                    if os.path.isfile(dockerFullPath) and \
                            os.stat(dockerFullPath).st_ino == int(ino):
                        return dockerFullPath+"[containterId:%s]" % cid

            except (IOError, EOFError) as e:
                continue
    except Exception:
        pass
    return "-"


def getMntPath(fileInfoDict):
    mntfname = fileInfoDict['mntfname']
    fsmountInfo = fileInfoDict['fsmountinfo']

    if len(fsmountInfo) <= 0:
        return '-'

    if mntfname.isspace() or len(mntfname) == 0:
        return fsmountInfo[0].split()[1]
    try:
        for l in fsmountInfo:
            if l.find(mntfname) > -1:
                return l.split()[1]
        return '-'
    except IndexError:
        return fsmountInfo[0].split()[1]


def getFullName(fileInfoDict):
    fileSuffix = ''

    mntdir = getMntPath(fileInfoDict)
    if mntdir == '/':
        mntdir = ''

    for f in [
        fileInfoDict['d3fname'], fileInfoDict['d2fname'],
        fileInfoDict['d1fname'], fileInfoDict['bfname']]:
        if f != '/' and f != '(fault)':
            fileSuffix += ('/' + f)
    if fileInfoDict['d3fname'] != '/' and fileInfoDict['d3fname'] != '(fault)':
        fileSuffix = '/...' + fileSuffix
    filename = mntdir + fileSuffix

    if '...' in filename:
        f = getFullNameFromProcPid(
            fileInfoDict['pid'], fileInfoDict['ino'])
        if f != '-':
            filename = f
    return filename


def echoFile(filename, txt):
    execCmd("echo \'"+txt+"\' > "+filename)


def echoFileAppend(filename, txt):
    execCmd("echo \'"+txt+"\' >> "+filename)


def supportKprobe(name):
    file = '/sys/kernel/debug/tracing/available_filter_functions'
    with open(file) as f:
        ss = f.read()
    if ss.find(name) > 0:
        return True
    return False


class fsstatClass(diskstatClass):
    def __init__(
        self, devname, pid, utilThresh, bwThresh, top,
        json, nodiskStat, miscStat, Pattern):
        super(fsstatClass, self).__init__(
            devname, utilThresh, json, nodiskStat, Pattern)
        self.expression = []
        self.pid = pid
        self.miscStat = miscStat
        self.devname = devname
        self.top = int(top) if top is not None else 99999999
        self.bwThresh = int(bwThresh) if bwThresh is not None else 0
        self.devt = getDevtRegion(devname) if devname is not None else [-1, -1]
        tracingBaseDir = "/sys/kernel/debug/tracing"
        self.kprobeEvent = tracingBaseDir+"/kprobe_events"
        self.tracingDir = tracingBaseDir+'/instances/iofsstat4fs'
        self.kprobeDir = self.tracingDir+"/events/kprobes"
        self.kprobe = []
        self.kprobeArgsFormat = 'dev=+0x10(+0x28(+0x20(%s))):u32 '\
            'inode_num=+0x40(+0x20(%s)):u64 len=%s:u64 '\
            'mntfname=+0x0(+0x28(+0x0(+0x10(%s)))):string '\
            'bfname=+0x0(+0x28(+0x18(%s))):string '\
            'd1fname=+0x0(+0x28(+0x18(+0x18(%s)))):string '\
            'd2fname=+0x0(+0x28(+0x18(+0x18(+0x18(%s))))):string '\
            'd3fname=+0x0(+0x28(+0x18(+0x18(+0x18(+0x18(%s)))))):string %s'

        kprobeArgs = self._getKprobeArgs('None')
        self.ftracePaserCommArgs = ' comm=(.*)' if 'comm=' in kprobeArgs else ''
        mmapKprobeArgs = self._getKprobeArgs('mmap')
        self.fsmountInfo = self._getFsMountInfo()
        for entry in self.fsmountInfo:
            fstype = entry.split()[2]
            self._kprobeReadWrite(fstype, kprobeArgs)
            self._kprobeMmap(fstype, mmapKprobeArgs)
        self.outlogFormatBase = 10

    def _kprobeReadWrite(self, fstype, kprobeArgs):
        for op in ['write', 'read']:
            kPoints = [
                fstype+"_file_"+op+"_iter", fstype+"_file_"+op,
                fstype+"_file_aio_"+op, "generic_file_aio_"+op]
            if list(set(self.kprobe) & set(kPoints)):
                continue
            kprobe = None
            for k in kPoints:
                if supportKprobe(k):
                    kprobe = k
                    break
            if not kprobe:
                print("not available %s kprobe" % op)
                sys.exit(0)
            pointKprobe = 'p '+kprobe+' '+kprobeArgs
            self.kprobe.append(kprobe)
            self.expression.append(pointKprobe)

    def _kprobeMmap(self, fstype, kprobeArgs):
        for kprobe in [fstype+"_page_mkwrite", 'filemap_fault']:
            if kprobe in self.kprobe:
                continue
            if not supportKprobe(kprobe):
                print("not support kprobe %s" % kprobe)
                continue
            pointKprobe = 'p '+kprobe+' '+kprobeArgs
            self.kprobe.append(kprobe)
            self.expression.append(pointKprobe)

    def _getKprobeArgs(self, type):
        commArgs = ''
        vinfo = execCmd('uname -r')
        version = vinfo.split('.')

        if type == 'mmap':
            offFile = '0xa0(+0x0%s)' if int(version[0]) > 4 or (
                int(version[0]) == 4 and int(version[1]) > 10) else '0xa0%s'
            offLen = '0x0(+0x0%s)' if int(version[0]) > 4 or (
                int(version[0]) == 4 and int(version[1]) > 10) else '0x0%s'
        else:
            offLen = '0x10'
            offFile = '0x0' if int(version[0]) > 3 or (
                int(version[0]) == 3 and int(version[1]) > 10) else '0x8'
            if int(version[0]) <= 3:
                offLen = '0x8' if int(version[1]) < 13 else '0x18'

        if int(version[0]) > 3:
            commArgs = 'comm=$comm'

        arch = execCmd('lscpu | grep Architecture').split(":", 1)[1].strip()
        regs = {
            "arm":['(%r0)','(%r1)'],
            "x86":['(%di)', '(%si)'],
            "aarch64":['(%x0)','(%x1)']}
        argv0 = argv1 = ''
        for key,val in regs.items():
            if arch.startswith(key):
                if type == 'mmap':
                    argv0 = '+' + (offFile % val[0])
                    argv1 = '+' + (offLen % val[0])
                    argv2 = argv1
                else:
                    argv2 = argv0 = '+' + offFile + val[0]
                    argv1 = '+' + offLen + val[1]
                break
        if argv0 == '':
            raise ValueError('arch %s not support' % arch)

        kprobeArgs = self.kprobeArgsFormat % (
            argv0, argv0, argv1, argv0, argv0, argv0, argv0, argv2, commArgs)
        return kprobeArgs

    def _getFsMountInfo(self):
        devList = []
        if self.devname is not None:
            devList.append('/dev/'+self.devname)
        else:
            sysfsBlockDirList = os.listdir("/sys/block")
            for dev in sysfsBlockDirList:
                devList.append('/dev/'+dev)
        with open("/proc/mounts") as f:
            fsmountInfo = list(filter(lambda x: any(
                e in x for e in devList), f.readlines()))
        return fsmountInfo

    def config(self):
        devt = self.devt

        if not os.path.exists(self.tracingDir):
            os.mkdir(self.tracingDir)
        for exp in self.expression:
            probe = 'p_'+exp.split()[1]+'_0'
            enableKprobe = self.kprobeDir+"/"+probe+"/enable"
            filterKprobe = self.kprobeDir+"/"+probe+"/filter"
            if os.path.exists(enableKprobe):
                echoFile(enableKprobe, "0")
                if devt[0] > 0:
                    echoFile(filterKprobe, "0")
                echoFileAppend(self.kprobeEvent, '-:%s' % probe)

            echoFileAppend(self.kprobeEvent, exp)
            if devt[0] > 0:
                dev = getDevt(self.devname)
                if dev == min(devt):
                    echoFile(filterKprobe,
                        "dev>="+str(min(devt))+"&&dev<="+str(max(devt)))
                else:
                    echoFile(filterKprobe, "dev=="+str(dev))
            echoFile(enableKprobe, "1")
            fmt = execCmd("grep print "+self.kprobeDir+"/"+probe+"/format")
            matchObj = re.match(r'(.*) dev=(.*) inode_num=(.*)', fmt)
            if 'x' in matchObj.group(2):
                self.outlogFormatBase = 16

    def start(self):
        echoFile(self.tracingDir+"/trace", "")
        echoFile(self.tracingDir+"/tracing_on", "1")
        super(fsstatClass, self).start()

    def stop(self):
        echoFile(self.tracingDir+"/tracing_on", "0")
        super(fsstatClass, self).stop()

    def clear(self):
        for exp in self.expression:
            probe = 'p_'+exp.split()[1]+'_0'
            enableKprobe = self.kprobeDir+"/"+probe+"/enable"
            if not os.path.exists(enableKprobe):
                continue
            echoFile(enableKprobe, "0")
            if self.devt[0] > 0:
                filterKprobe = self.kprobeDir+"/"+probe+"/filter"
                echoFile(filterKprobe, "0")
            echoFileAppend(self.kprobeEvent, '-:%s' % probe)
        super(fsstatClass, self).clear()

    def _paserTraceToStat(self, traceText):
        bwTotal = 0
        stat = {}
        mStat = {}
        fileInfoDict = {
            'device': 0, 'mntfname': '', 'bfname': '', 'd1fname': '',
            'd2fname': '', 'd3fname': '', 'fsmountinfo': '', 'ino': 0,
            'pid': 0}
        commArgs = self.ftracePaserCommArgs
        hasCommArgs = True if len(commArgs) else False

        # pool-1-thread-2-5029  [002] .... 5293018.252338: p_ext4_file_write_iter_0:\
        # (ext4_file_write_iter+0x0/0x6d0 [ext4]) dev=265289729 inode_num=530392 len=38
        # ...
        for entry in traceText:
            if ('dev=' not in entry) or ('=\"etc\"' in entry) or (
                '=\"usr\"' in entry and (
                '=\"bin\"' in entry or '=\"sbin\"' in entry)):
                continue

            matchObj = re.match(
                    r'(.*) \[([^\[\]]*)\] (.*) dev=(.*) inode_num=(.*) len=(.*)'+
                    ' mntfname=(.*) bfname=(.*) d1fname=(.*) d2fname=(.*)'+
                    ' d3fname=(.*)'+commArgs, entry)
            if matchObj is None or ('.so' in matchObj.group(8).strip("\"") and\
                'lib' in matchObj.group(9).strip("\"") and \
                'usr' in matchObj.group(10).strip("\"")):
                continue

            pid = (matchObj.group(1).rsplit('-', 1))[1].strip()
            dev = int(matchObj.group(4), self.outlogFormatBase)
            if (self.pid is not None and int(pid) != self.pid) or \
                str(dev) == '0':
                continue

            if hasCommArgs:
                comm = matchObj.group(12).strip("\"")
            else:
                comm = (matchObj.group(1).rsplit('-', 1))[0].strip()
            comm = fixComm(comm, pid)
            if '..' in comm:
                continue

            device = self.getDevNameByDevt(dev)
            if device == '-':
                continue
            if self.miscStat is not None:
                disk = self.getMasterDev(dev)
                if not mStat.has_key(disk):
                    mStat.setdefault(disk, {})
                stat = mStat[disk]

            ino = int(matchObj.group(5), self.outlogFormatBase)
            inoTask = str(ino)+':'+str(comm)+':'+device
            if not stat.has_key(inoTask):
                fsmountinfo = [f for f in self.fsmountInfo if ('/dev/'+device) in f]
                fileInfoDict['device'] = device
                fileInfoDict['mntfname'] = matchObj.group(7).strip("\"")
                fileInfoDict['bfname'] = matchObj.group(8).strip("\"")
                fileInfoDict['d1fname'] = matchObj.group(9).strip("\"")
                fileInfoDict['d2fname'] = matchObj.group(10).strip("\"")
                fileInfoDict['d3fname'] = matchObj.group(11).strip("\"")
                fileInfoDict['fsmountinfo'] = fsmountinfo
                fileInfoDict['ino'] = ino
                fileInfoDict['pid'] = pid
                stat.setdefault(inoTask,
                    {"inode":str(ino), "comm": comm, "tgid": getTgid(pid), "pid": pid,
                    "cnt_wr": 0, "bw_wr": 0, "cnt_rd": 0, "bw_rd": 0, "device": device,
                    "file": getFullName(fileInfoDict)})

            size = int(matchObj.group(6), self.outlogFormatBase)
            if 'filemap_fault' in entry or 'page_mkwrite' in entry:
                size = PAGESIZE
            if 'write' in entry or 'page_mkwrite' in entry:
                stat[inoTask]["cnt_wr"] += 1
                stat[inoTask]["bw_wr"] += int(size)
            if 'read' in entry or 'filemap_fault' in entry:
                stat[inoTask]["cnt_rd"] += 1
                stat[inoTask]["bw_rd"] += int(size)
            if pid != stat[inoTask]["pid"]:
                stat[inoTask]["pid"] = pid
                stat[inoTask]["tgid"] = getTgid(pid)
            bwTotal += int(size)
        return bwTotal,stat,mStat

    def _joinMiscStat(self, mStat):
        for d,val in self.miscStat:
            if d not in mStat.keys():
                mStat.setdefault(d, {})
            mStat[d].update(dict(val))
        tmpStat = []
        for d,val in mStat.items():
            idxSort = 'bw_wr'
            if self.getDiskStatInd(d, 'w_iops') < self.getDiskStatInd(d, 'r_iops'):
                idxSort = 'bw_rd'
            s = sorted(
                val.items(), key=lambda e: (e[1][idxSort]), reverse=True)[:self.top]
            tmpStat.append((d, s))
        del self.miscStat[:]
        self.miscStat.extend(tmpStat)
        return 0

    def showJson(self, stat):
        secs = self.cycle
        statJsonStr = '{"time":"","fsstats":[]}'
        fstatDicts = json.loads(statJsonStr, object_pairs_hook=OrderedDict)
        fstatDicts['time'] = time.strftime(
            '%Y/%m/%d %H:%M:%S', time.localtime())
        stSecs = str(secs)+'s' if secs > 1 else 's'
        for key, item in stat.items():
            if (item["cnt_wr"] + item["cnt_rd"]) == 0:
                continue
            item["bw_wr"] = \
                humConvert(item["bw_wr"], True).replace('s', stSecs) if item["bw_wr"] else 0
            item["bw_rd"] = \
                humConvert(item["bw_rd"], True).replace('s', stSecs) if item["bw_rd"] else 0
            fsstatJsonStr = '{\
                "inode":0,"comm":"","tgid":0,"pid":0,"cnt_rd":0,\
                "bw_rd":0,"cnt_wr":0,"bw_wr":0,"device":0,"file":0}'
            fsstatDict = json.loads(
                fsstatJsonStr, object_pairs_hook=OrderedDict)
            for key, val in item.items():
                fsstatDict[key] = val
            fstatDicts["fsstats"].append(fsstatDict)
        if len(fstatDicts["fsstats"]) > 0:
            self.writeDataToJson(json.dumps(fstatDicts))

    def printStat(self, stat):
        secs = self.cycle
        print("%-20s%-8s%-8s%-8s%-12s%-8s%-12s%-12s%-12s%s"
              % ("comm", "tgid", "pid", "cnt_rd", "bw_rd",
                 "cnt_wr", "bw_wr", "inode", "device", "filepath"))
        stSecs = str(secs)+'s' if secs > 1 else 's'
        for key, item in stat:
            if (item["cnt_wr"] + item["cnt_rd"]) == 0:
                continue
            item["bw_wr"] = \
                humConvert(item["bw_wr"], True).replace('s', stSecs) if item["bw_wr"] else 0
            item["bw_rd"] = \
                humConvert(item["bw_rd"], True).replace('s', stSecs) if item["bw_rd"] else 0
            print("%-20s%-8s%-8s%-8d%-12s%-8d%-12s%-12s%-12s%s"
                  % (item["comm"], item["tgid"], item["pid"], item["cnt_rd"],
                  item["bw_rd"], item["cnt_wr"], item["bw_wr"], item["inode"],
                  item["device"], item["file"]))
        print("")

    def show(self):
        secs = self.cycle
        with open(self.tracingDir+"/trace") as f:
            traceText = f.read().split('\n')
            #traceText = f.readlines()
            #traceText = \
            #    list(filter(lambda x: any(e in x for e in self.kprobe), f.readlines()))
        bwTotal,stat,mStat = self._paserTraceToStat(traceText)

        if self.miscStat is not None:
            return self._joinMiscStat(mStat)
        elif (self.bwThresh and (bwTotal/secs) < self.bwThresh):
            return

        stat = sorted(stat.items(), key=lambda e: (
                e[1]["bw_wr"]+e[1]["bw_rd"]), reverse=True)[:self.top]

        if self.enableJsonShow() == False:
            print(time.strftime('%Y/%m/%d %H:%M:%S', time.localtime()))
        if self.disableShow() == False:
            super(fsstatClass, self).show()

        if self.enableJsonShow() == True:
            self.showJson(stat)
        else:
            self.printStat(stat)

    def entry(self, interval):
        self.start()
        time.sleep(float(interval))
        self.stop()
        self.show()
