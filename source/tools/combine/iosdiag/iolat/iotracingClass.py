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
from subprocess import PIPE, Popen


def execCmd(cmd):
    p = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
    return p.stdout.read().decode('utf-8')


def echoFile(filename, txt):
    execCmd("echo \""+txt+"\" > "+filename)


def echoFileAppend(filename, txt):
    execCmd("echo \'"+txt+"\' >> "+filename)


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

class iotracingClass():
    def __init__(self, devname, thresh, log):
        self.devt = min(getDevtRegion(devname)) if devname is not None else -1
        self.devname = devname
        self.thresh = int(thresh) if thresh else 5000
        self.kprobeEvent = "/sys/kernel/debug/tracing/kprobe_events"
        self.tracingDir = "/sys/kernel/debug/tracing/instances/iotracing"
        self.blkTraceDir = self.tracingDir+"/events/block"
        self.kprobeDir = self.tracingDir+"/events/kprobes"
        self.tracepoints = \
            ['block_getrq', 'block_rq_insert', 'block_rq_issue',
            'block_rq_complete']
        arch = execCmd('lscpu | grep Architecture').split(":", 1)[1].strip()
        regs = {
            "arm":['%r0','%r1'], "x86":['%di', '%si'], "aarch64":['%x0','%x1']}
        argv0 = argv1 = ''
        for key,val in regs.items():
            if arch.startswith(key):
                argv0 = val[0]
                argv1 = val[1]
                break
        if argv0 == '':
            raise ValueError('arch %s not support' % arch)
        kprobepoints = [
            ('p scsi_dispatch_cmd dev=+0xc(+0xc8(+0x100(%s))):string'\
                ' sector=+0x68(+0x100(%s)):u64' %(argv0, argv0),
             'p scsi_done dev=+0xc(+0xc8(+0x100(%s))):string'\
                ' sector=+0x68(+0x100(%s)):u64' %(argv0, argv0)),
            ('p nvme_queue_rq rq=+0x0(%s):u64 kick=+0x10(%s):u8'\
                ' dev=+0xc(+0xc8(+0x0(%s))):string'\
                ' sector=+0x68(+0x0(%s)):u64' %(argv1, argv1, argv1, argv1),
             'p blk_mq_complete_request rq=%s:u64 sector=+0x68(%s):u64' %(
                 argv0, argv0)),
            ('p virtio_queue_rq rq=+0x0(%s):u64 kick=+0x10(%s):u8'\
                ' dev=+0xc(+0xc8(+0x0(%s))):string'\
                ' sector=+0x68(+0x0(%s)):u64' %(argv1, argv1, argv1, argv1),
             'p blk_mq_complete_request rq=%s:u64 sector=+0x68(%s):u64' %(
                 argv0, argv0)),
            ('p blk_mq_complete_request rq=+0x0(%s):u64 '\
                ' sector=+0x68(+0x0(%s)):u64' %(argv0, argv0),
             'p blk_account_io_done dev=+0xc(+0xc8(%s)):string'\
                ' sector=+0x68(%s):u64' %(
                 argv0, argv0))]
        try:
            link = os.readlink('/sys/class/block/'+devname)
        except Exception:
            link = "none"
        module = [l for l in ["virtio", "nvme", "target"] if l in link]
        mod = module[0] if len(module) > 0 else 'none'
        if mod == 'target':
            mod = 'scsi'
        self.kprobepoints = []
        for k in kprobepoints:
            if mod in k[0] or mod == 'none':
                self.kprobepoints = list(set(self.kprobepoints+list(k)))
        if mod == 'none':
            self.devname = None

        #self.stage = {
        #    "1":"os(block_G2I)", "2":"os(block_I2D)", "3":"os(driver)",
        #    "4":"disk", "5":"os(complete)"}
        self.stage = {
            "1":"os(block)", "2":"os(driver)", "3":"disk", "4":"os(complete)", "5":"os(done)"}
        self.pointsCnt = len(self.stage) + 1
        self.diskInfoDicts = {}
        diskList = os.listdir('/sys/class/block/')
        for disk in diskList:
            try:
                with open('/sys/class/block/' + disk + '/dev') as f:
                    dev = f.read().split(':')
                    devt = (int(dev[0]) << 20) + int(dev[1])
                    self.diskInfoDicts[str(devt)] = disk
            except Exception:
                continue
        if not log:
            log = '/var/log/sysak/iosdiag/latency/result.log.seq'
        logdir = os.path.dirname(log)
        if not os.path.exists(logdir):
            os.mkdir(logdir)
        self.fJson = open(log, 'w+')
        pattern = r'\[([^\]]*\s)'
        result = \
            re.sub(pattern, '', execCmd('echo test > /dev/kmsg;cat /proc/uptime;dmesg | tail -1'))
        txt = re.split(' |\n',result)
        self.timeDiff = \
            float(txt[0]) - float(txt[2].strip('[').strip(']'))

    def writeDataToJson(self, data):
        self.fJson.write(data+'\n')

    def getDevNameByDevt(self, devt):
        try:
            return self.diskInfoDicts[str(devt)]
        except Exception:
            return '-'

    def config(self):
        devt = self.devt
        if not os.path.exists(self.tracingDir):
            os.mkdir(self.tracingDir)
        for point in self.tracepoints:
            if devt > 0:
                echoFile(self.blkTraceDir+"/"+point+"/filter", "dev=="+str(devt))
            else:
                echoFile(self.blkTraceDir+"/"+point+"/filter", "")
            echoFile(self.blkTraceDir+"/"+point+"/enable", "1")

        for exp in self.kprobepoints:
            probe = 'p_'+exp.split()[1]+'_0'
            enableKprobe = self.kprobeDir+"/"+probe+"/enable"
            filterKprobe = self.kprobeDir+"/"+probe+"/filter"
            if os.path.exists(enableKprobe):
                echoFile(enableKprobe, "0")
                if devt > 0:
                    echoFile(filterKprobe, "0")
                echoFileAppend(self.kprobeEvent, '-:%s' % probe)

            echoFileAppend(self.kprobeEvent, exp)
            if devt > 0:
                if 'dev=' in exp:
                    echoFile(filterKprobe, "dev==\""+self.devname+"\"")
            else:
                echoFile(filterKprobe, "")
            echoFile(enableKprobe, "1")

    def start(self):
        echoFile(self.tracingDir+"/trace", "")
        echoFile(self.tracingDir+"/tracing_on", "1")

    def stop(self):
        echoFile(self.tracingDir+"/tracing_on", "0")

    def clear(self):
        for point in self.tracepoints:
            echoFile(self.blkTraceDir+"/"+point+"/enable", "0")
            if self.devt > 0:
                echoFile(self.blkTraceDir+"/"+point+"/filter", "0")

        for exp in self.kprobepoints:
            probe = 'p_'+exp.split()[1]+'_0'
            enableKprobe = self.kprobeDir+"/"+probe+"/enable"
            if not os.path.exists(enableKprobe):
                continue
            echoFile(enableKprobe, "0")
            if self.devt > 0:
                filterKprobe = self.kprobeDir+"/"+probe+"/filter"
                echoFile(filterKprobe, "0")
            echoFileAppend(self.kprobeEvent, '-:%s' % probe)
        self.fJson.close()

    def paserBlockTracepoints(self, oneIO, ios, point):
        commSpaceCnt = 0
        for i in range(len(oneIO)-1, -1, -1):
            if oneIO[i].startswith('['):
                break
            commSpaceCnt += 1
        refIdx = oneIO.index(point+':')
        time = float(oneIO[refIdx-1].strip(':'))*1000000
        devinfo = oneIO[refIdx+1].split(',')
        dev = ((int(devinfo[0]) << 20) + int(devinfo[1]))
        diskname = self.getDevNameByDevt(dev)
        if diskname == '-':
            return
        sector = int(oneIO[-4-commSpaceCnt])
        key = str(sector)+':'+diskname
        if point != 'block_getrq':
            try:
                ios[key]["t"][point] = time
            except Exception:
                pass
            return
        cpu = int(oneIO[refIdx-3].lstrip('[').rstrip(']'))
        comm = ' '.join(
            oneIO[(len(oneIO)-1-commSpaceCnt):]).lstrip('[').rstrip(']')
        pid = oneIO[refIdx-4].rsplit('-', 1)[1].strip()
        iotype = oneIO[refIdx+2]
        size = int(oneIO[-2-commSpaceCnt]) * 512
        ios[key] = {
            "comm":comm, "pid":pid, "sector":sector, "diskname":diskname,
            "datalen":size, "iotype":iotype, "cpu":cpu,
            "t":OrderedDict({point:time})}

    def paserKbprobepoints(self, oneIO, ios, point):
        # <...>-72446 [003] .... 52607435.533155: block_getrq: 253,0 W 74298000 + 8 [kworker/u8:3]
        kick = 1
        diskname = \
            oneIO[-2].split('=')[1].strip('\"') if 'dev' in oneIO[-2] else '-'
        refIdx = oneIO.index(point+':')
        time = float(oneIO[refIdx-1].strip(':'))*1000000
        sector = oneIO[-1].split('=')[1]
        try:
            sector = int(sector, 16)
        except ValueError:
            sector = int(sector)
        key = str(sector)+':'+diskname
        if 'rq=' in oneIO[-2] or 'rq=' in oneIO[-4]:
            rqField = oneIO[-2] if 'rq=' in oneIO[-2] else oneIO[-4]
            rq = rqField.split('=')[1]
            keyR = str(sector)+':'+rq
            if 'queue_rq' in point:
                kick = int(oneIO[-3].split('=')[1])
                ios['keyR'][keyR] = key
            try:
                key = ios['keyR'][keyR]
            except Exception:
                pass
        if kick:
            try:
                ios[key]["t"][point] = time
            except Exception:
                pass

    def showJson(self, ios):
        del ios['keyR']
        for io in ios.values():
            if len(io['t']) < self.pointsCnt:
                continue
            io['t'] = io['t'].values()
            total_delay = io["t"][-1] - io["t"][0]
            if (self.thresh*1000) > total_delay:
                continue
            with open('/proc/uptime') as f:
                uptime = float(f.read().split()[0])
            now = time.time()
            timestamp = now - uptime + io['t'][0]/1000000.0 + self.timeDiff
            startTime = time.strftime(
                '%Y-%m-%d %H:%M:%S', time.localtime(timestamp))
            startTime = startTime+'.'+str(timestamp).split('.')[1][:3]
            io["time"] = startTime
            delay = {
                "time":startTime, "diskname":io["diskname"],
                "totaldelay":total_delay, "delays":[]}
            max_delay = 0
            for i in range(1, len(io["t"]), 1):
                d = io["t"][i] - io["t"][i-1]
                if d >= max_delay:
                    max_delay = d
                    component = self.stage[str(i)]
                delay["delays"].append(
                    {"component":self.stage[str(i)], "delay":d})
            io["abnormal"] = (
                '%s delay (%d:%d us)' % (
                    component, max_delay, total_delay))
            del io["t"]
            self.writeDataToJson(
                json.dumps(io)+'\n'+json.dumps(delay))

    def show(self):
        ios = {}
        ios['keyR'] = {}
        with open(self.tracingDir+"/trace") as f:
            traceText = f.readlines()
        # <...>-72446 [003] .... 52607435.533155: block_getrq: 253,0 W 74298000 + 8 [kworker/u8:3]
        for entry in traceText:
            oneIO = entry.split()
            if len(oneIO) < 1:
                return
            if 'block_getrq' in entry:
                self.paserBlockTracepoints(oneIO, ios, 'block_getrq')
            elif 'block_rq_insert' in entry:
                #self.paserBlockTracepoints(oneIO, ios, 'block_rq_insert')
                pass
            elif 'block_rq_issue' in entry:
                self.paserBlockTracepoints(oneIO, ios, 'block_rq_issue')
            elif 'scsi_dispatch_cmd' in entry:
                self.paserKbprobepoints(oneIO, ios, 'p_scsi_dispatch_cmd_0')
            elif 'scsi_done' in entry:
                self.paserKbprobepoints(oneIO, ios, 'p_scsi_done_0')
            elif 'nvme_queue_rq' in entry:
                self.paserKbprobepoints(oneIO, ios, 'p_nvme_queue_rq_0')
            elif 'virtio_queue_rq' in entry:
                self.paserKbprobepoints(oneIO, ios, 'p_virtio_queue_rq_0')
            elif 'blk_mq_complete_request' in entry:
                self.paserKbprobepoints(oneIO, ios, 'p_blk_mq_complete_request_0')
            elif 'block_rq_complete' in entry:
                self.paserBlockTracepoints(oneIO, ios, 'block_rq_complete')
            elif 'blk_account_io_done' in entry:
                self.paserKbprobepoints(oneIO, ios, 'p_blk_account_io_done_0')
        self.showJson(ios)


    def entry(self, interval):
        self.start()
        time.sleep(float(interval))
        self.stop()
        self.show()
