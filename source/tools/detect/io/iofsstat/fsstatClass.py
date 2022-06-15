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
from diskstatClass import getDevt
from diskstatClass import humConvert


def execCmd(cmd):
    r = os.popen(cmd+" 2>/dev/null")
    text = r.read()
    r.close()
    return text


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
                dockerRootPath = execCmd("docker inspect -f '{{.HostRootPath}}' "+cid)\
                    .strip('\n')
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
        return "-"
    return "-"


def getMntPath(mntfname, fsmountInfo):
    if mntfname.isspace() or len(mntfname) == 0:
        return fsmountInfo[0].split()[1]
    try:
        for l in fsmountInfo:
            if l.find(mntfname) > -1:
                return l.split()[1]
    except IndexError:
        return fsmountInfo[0].split()[1]


def getFullName(fileInfoDict):
    filename = getFullNameFromProcPid(fileInfoDict['pid'], fileInfoDict['ino'])
    if filename == '-':
        mntfname = fileInfoDict['mntfname']
        fsmountInfo = fileInfoDict['fsmountinfo']
        bfname = fileInfoDict['bfname']
        d1fname = fileInfoDict['d1fname']
        d2fname = fileInfoDict['d2fname']
        d3fname = fileInfoDict['d3fname']
        if len(fsmountInfo) != 0:
            mntdir = getMntPath(mntfname, fsmountInfo)
        else:
            mntdir = '-'

        fileSuffix = ''
        if d1fname == '/':
            fileSuffix = '/'+bfname
        else:
            if d2fname == '/':
                fileSuffix = '/'+d1fname+'/'+bfname
            else:
                if d3fname == '/':
                    fileSuffix = '/'+d2fname+'/'+d1fname+'/'+bfname
                else:
                    fileSuffix = '/.../'+d3fname+d2fname+'/'+d1fname+'/'+bfname
        if mntdir == '/':
            mntdir = ''
        if mntdir is None:
            mntdir = '-'
        filename = mntdir+fileSuffix
    return filename


def echoFile(filename, txt):
    execCmd("echo \""+txt+"\" > "+filename)


def echoFileAppend(filename, txt):
    execCmd("echo \""+txt+"\" >> "+filename)


def supportKprobe(name):
    cmd = "cat /sys/kernel/debug/tracing/available_filter_functions |grep " + name
    ss = execCmd(cmd).strip()
    for res in ss.split('\n'):
        if ':' in res:
            res = res.split(":", 1)[1]
        if ' [' in res:  # for ko symbol
            res = res.split(" [", 1)[0]
        if res == name:
            return True
    return False


class fsstatClass(diskstatClass):
    def __init__(self, devname, pid, utilThresh, cycle, bwThresh, top, json):
        super(fsstatClass, self).__init__(devname, utilThresh, cycle, json)
        self.expression = []
        self.pid = pid
        self.devname = devname
        self.top = int(top) if top is not None else 99999999
        self.bwThresh = int(bwThresh) if bwThresh is not None else 0
        self.devt = getDevt(self.devname) if devname is not None else -1
        tracingBaseDir = "/sys/kernel/debug/tracing"
        self.kprobeEvent = tracingBaseDir+"/kprobe_events"
        self.tracingDir = tracingBaseDir+'/instances/iofsstat'
        self.kprobeDir = self.tracingDir+"/events/kprobes"
        self.kprobe = []
        self.inputExp = []
        vinfo = execCmd('uname -r')

        version = vinfo.split('.')
        offflip = '0x0' if int(version[0]) > 3 or (
            int(version[0]) == 3 and int(version[1]) > 10) else '0x8'
        offlen = '0x10'
        if int(version[0]) <= 3:
            offlen = '0x8' if 'ali' in vinfo else '0x18'
        arch = execCmd('lscpu | grep Architecture').split(
            ":", 1)[1].strip()
        if arch.startswith("arm"):
            argv0 = '+'+offflip+'(%r0)'
            argv1 = '+'+offlen+'(%r1)'
        elif arch.startswith("x86"):
            argv0 = '+'+offflip+'(%di)'
            argv1 = '+'+offlen+'(%si)'
        elif arch.startswith("aarch64"):
            argv0 = '+'+offflip+'(%x0)'
            argv1 = '+'+offlen+'(%x1)'
        else:
            raise ValueError('arch %s not support' % arch)
        kprobeArgs = 'dev=+0x10(+0x28(+0x20(%s))):u32 '\
            'inode_num=+0x40(+0x20(%s)):u64 len=%s:u64 '\
            'mntfname=+0x0(+0x28(+0x0(+0x10(%s)))):string '\
            'bfname=+0x0(+0x28(+0x18(%s))):string '\
            'd1fname=+0x0(+0x28(+0x18(+0x18(%s)))):string '\
            'd2fname=+0x0(+0x28(+0x18(+0x18(+0x18(%s))))):string '\
            'd3fname=+0x0(+0x28(+0x18(+0x18(+0x18(+0x18(%s)))))):string ' % \
            (argv0, argv0, argv1, argv0, argv0, argv0, argv0, argv0)

        devList = []
        if self.devname is not None:
            devList.append('/dev/'+self.devname)
        else:
            sysfsBlockDirList = os.listdir("/sys/block")
            for dev in sysfsBlockDirList:
                devList.append('/dev/'+dev)
        with open("/proc/mounts") as f:
            self.fsmountInfo = list(filter(lambda x: any(
                e in x for e in devList), f.readlines()))

        for entry in self.fsmountInfo:
            fstype = entry.split()[2]
            kprobe = fstype+"_file_write_iter"
            if kprobe in self.kprobe:
                continue
            if supportKprobe(kprobe):
                pass
            elif supportKprobe(fstype+"_file_write"):
                kprobe = fstype+"_file_write"
            elif supportKprobe(fstype+"_file_aio_write"):
                kprobe = fstype+"_file_aio_write"
            elif supportKprobe("generic_file_aio_write"):
                kprobe = "generic_file_aio_write"
            else:
                print("not available write kprobe")
                sys.exit(0)
            writeKprobe = 'p '+kprobe+' '+kprobeArgs
            self.kprobe.append(kprobe)
            self.inputExp.append(writeKprobe)

            kprobe = fstype+"_file_read_iter"
            if supportKprobe(kprobe):
                pass
            elif supportKprobe(fstype+"_file_read"):
                kprobe = fstype+"_file_read"
            elif supportKprobe(fstype+"_file_aio_read"):
                kprobe = fstype+"_file_aio_read"
            elif supportKprobe("generic_file_aio_read"):
                kprobe = "generic_file_aio_read"
            else:
                print("not available read kprobe")
                sys.exit(0)
            readKprobe = 'p '+kprobe+' '+kprobeArgs
            self.kprobe.append(kprobe)
            self.inputExp.append(readKprobe)

            self.expression = self.inputExp
        self.outlogFormatBase = 10

    def config(self):
        devt = self.devt

        if not os.path.exists(self.tracingDir):
            os.mkdir(self.tracingDir)
        for exp in self.expression:
            probe = 'p_'+exp.split()[1]+'_0'
            enableKprobe = self.kprobeDir+"/"+probe+"/enable"
            if os.path.exists(enableKprobe):
                echoFile(enableKprobe, "0")
                if devt > 0:
                    filterKprobe = self.kprobeDir+"/"+probe+"/filter"
                    echoFile(filterKprobe, "0")
                echoFileAppend(self.kprobeEvent, '-:%s' % probe)

            echoFileAppend(self.kprobeEvent, exp)
            if devt > 0:
                echoFile(filterKprobe, "dev=="+str(devt))
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
            echoFile(enableKprobe, "0")
            if self.devt > 0:
                filterKprobe = self.kprobeDir+"/"+probe+"/filter"
                echoFile(filterKprobe, "0")
            echoFileAppend(self.kprobeEvent, '-:%s' % probe)
        super(fsstatClass, self).clear()

    def showJson(self, stat):
        secs = self.cycle
        top = 0
        statJsonStr = '{\
			"time":"",\
			"fsstats":[]}'
        fstatDicts = json.loads(statJsonStr, object_pairs_hook=OrderedDict)
        fstatDicts['time'] = time.strftime(
            '%Y/%m/%d %H:%M:%S', time.localtime())
        for key, item in stat.items():
            if (item["cnt_wr"] + item["cnt_rd"]) == 0:
                continue
            if top >= self.top:
                break
            top += 1
            item["cnt_wr"] /= secs
            item["bw_wr"] = humConvert(item["bw_wr"]/secs, True)
            item["cnt_rd"] /= secs
            item["bw_rd"] = humConvert(item["bw_rd"]/secs, True)
            fsstatJsonStr = '{\
				"inode":0,\
				"comm":"",\
				"tgid":0,\
				"pid":0,\
				"cnt_rd":0,\
				"bw_rd":0,\
				"cnt_wr":0,\
				"bw_wr":0,\
				"device":0,\
				"file":0}'
            fsstatDict = json.loads(
                fsstatJsonStr, object_pairs_hook=OrderedDict)
            fsstatDict["inode"] = key
            for key, val in item.items():
                fsstatDict[key] = val
            fstatDicts["fsstats"].append(fsstatDict)
        if len(fstatDicts["fsstats"]) > 0:
            print(json.dumps(fstatDicts))

    def show(self):
        top = 0
        bwTotal = 0
        stat = {}
        secs = self.cycle
        with open(self.tracingDir+"/trace") as f:
            traceText = list(filter(
                lambda x: any(e in x for e in self.kprobe),
                f.readlines()))

        # pool-1-thread-2-5029  [002] .... 5293018.252338: p_ext4_file_write_iter_0:\
        # (ext4_file_write_iter+0x0/0x6d0 [ext4]) dev=265289729 inode_num=530392 len=38
        # ...
        for entry in traceText:
            matchObj = re.match(r'(.*) \[([^\[\]]*)\] (.*) dev=(.*) ' +
                                'inode_num=(.*) len=(.*) mntfname=(.*) ' +
                                'bfname=(.*) d1fname=(.*) d2fname=(.*) d3fname=(.*)\n', entry)
            # print(entry)
            if matchObj is not None:
                commInfo = matchObj.group(1).rsplit('-', 1)
            else:
                continue
            pid = commInfo[1].strip()
            if self.pid is not None and pid != self.pid:
                continue
            dev = int(matchObj.group(4), self.outlogFormatBase)
            if self.devt > 0 and self.devt != dev:
                continue
            device = super(fsstatClass, self).getDevNameByDevt(dev)
            # if super(fsstatClass, self).notCareDevice(device) == True:
            #	continue
            ino = int(matchObj.group(5), self.outlogFormatBase)
            if bool(stat.has_key(ino)) != True:
                comm = fixComm(commInfo[0].lstrip(), pid)
                if '..' in comm:
                    continue
                fsmountinfo = []
                for f in self.fsmountInfo:
                    if ('/dev/'+device) in f:
                        fsmountinfo.append(f)

                fileInfoDict = {
                    'device': device,
                    'mntfname': matchObj.group(7).strip("\""),
                    'bfname': matchObj.group(8).strip("\""),
                    'd1fname': matchObj.group(9).strip("\""),
                    'd2fname': matchObj.group(10).strip("\""),
                    'd3fname': matchObj.group(11).strip("\""),
                    'fsmountinfo': fsmountinfo,
                    'ino': ino, 'pid': pid}
                stat.setdefault(ino,
                                {"comm": comm, "tgid": getTgid(pid), "pid": pid,
                                 "cnt_wr": 0, "bw_wr": 0, "cnt_rd": 0, "bw_rd": 0,
                                 "device": device,
                                 "file": getFullName(fileInfoDict)})
            size = int(matchObj.group(6), self.outlogFormatBase)
            if 'write' in entry:
                stat[ino]["cnt_wr"] += 1
                stat[ino]["bw_wr"] += int(size)
            if 'read' in entry:
                stat[ino]["cnt_rd"] += 1
                stat[ino]["bw_rd"] += int(size)
            bwTotal += int(size)

        if (bwTotal/secs) < self.bwThresh or \
                ((self.bwThresh > 0 and
                  (bwTotal/secs) < super(fsstatClass, self).getDiskBW())):
            return

        if super(fsstatClass, self).enableJsonShow() == False:
            print(time.strftime('%Y/%m/%d %H:%M:%S', time.localtime()))
        if super(fsstatClass, self).disableShow() == False:
            super(fsstatClass, self).show()

        if stat:
            stat = OrderedDict(sorted(stat.items(),
                                      key=lambda e: (e[1]["bw_wr"]+e[1]["bw_rd"]), reverse=True))
        if super(fsstatClass, self).enableJsonShow() == True:
            self.showJson(stat)
            return

        print("%-20s%-8s%-8s%-8s%-12s%-8s%-12s%-12s%-12s%s"
              % ("comm", "tgid", "pid", "cnt_rd", "bw_rd",
                 "cnt_wr", "bw_wr", "inode", "device", "filepath"))
        for key, item in stat.items():
            if (item["cnt_wr"] + item["cnt_rd"]) == 0:
                continue
            if top >= self.top:
                break
            top += 1
            item["cnt_wr"] /= secs
            item["bw_wr"] = humConvert(item["bw_wr"]/secs, True)
            item["cnt_rd"] /= secs
            item["bw_rd"] = humConvert(item["bw_rd"]/secs, True)
            print("%-20s%-8s%-8s%-8d%-12s%-8d%-12s%-12s%-12s%s"
                  % (item["comm"], item["tgid"], item["pid"],
                     item["cnt_rd"], item["bw_rd"], item["cnt_wr"],
                     item["bw_wr"], key, item["device"], item["file"]))
        print("")

    def entry(self, interval):
        self.start()
        time.sleep(float(interval))
        self.stop()
        self.show()
