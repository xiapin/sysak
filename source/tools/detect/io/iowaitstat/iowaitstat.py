#!/usr/bin/python
# -*- coding: utf-8 -*-

import os
import sys
import signal
import string
import argparse
import time
import re
import json
from collections import OrderedDict

global_stop = False


def signal_exit_handler(signum, frame):
    global global_stop
    global_stop = True

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


def echoFile(filename, txt):
    execCmd("echo \'"+txt+"\' > "+filename)


def echoFileAppend(filename, txt):
    execCmd("echo \'"+txt+"\' >> "+filename)


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

class iowaitClass():
    def __init__(self, pid, cycle, top, json, iowait_thresh):
        self.pid = pid
        self.top = int(top) if top is not None else 99999999
        self.json = json
        self.cycle = cycle
        self.iowait_thresh = int(iowait_thresh) if iowait_thresh is not None else 0
        self.kprobeEvent = "/sys/kernel/debug/tracing/kprobe_events"
        self.tracingDir = "/sys/kernel/debug/tracing/instances/iowait"
        self.kprobeDir = self.tracingDir+"/events/kprobes"
        self.expression = []
        self.kprobe = []
        self.cpuStatIowait = {'sum': 0, 'iowait': 0}
        self.ftracePaserCommArgs = ''

        version = execCmd('uname -r').split('.')
        commArgs = ''
        if int(version[0]) > 3:
            commArgs = ' comm=$comm'
            self.ftracePaserCommArgs = ' comm=(.*)'

        for kprobe,retProbe in {'io_schedule_timeout':True, 'io_schedule':True}.items():
            if supportKprobe(kprobe) == False:
                print("not available %s kprobe" % kprobe)
                continue
            self.expression.append('p:p_%s_0 %s%s' % (kprobe, kprobe, commArgs))
            self.kprobe.append('p_%s_0' % kprobe)
            if retProbe == True:
                self.expression.append('r:r_%s_0 %s%s' % (kprobe, kprobe, commArgs))
                self.kprobe.append('r_%s_0' % kprobe)
        if len(self.kprobe) == 0:
            print "not available kprobe"
            sys.exit(0)

    def config(self):
        if not os.path.exists(self.tracingDir):
            os.mkdir(self.tracingDir)
        for exp in self.expression:
            probe = exp.split()[0].split(':')[1]
            enableKprobe = self.kprobeDir+"/"+probe+"/enable"
            if os.path.exists(enableKprobe):
                echoFile(enableKprobe, "0")
                echoFileAppend(self.kprobeEvent, '-:%s' % probe)

            echoFileAppend(self.kprobeEvent, exp)
            echoFile(enableKprobe, "1")

    def start(self):
        echoFile(self.tracingDir+"/trace", "")
        echoFile(self.tracingDir+"/tracing_on", "1")
        with open("/proc/stat") as fStat:
            cpuStatList = map(long, fStat.readline().split()[1:])
            self.cpuStatIowait['sum'] = sum(cpuStatList)
            self.cpuStatIowait['iowait'] = cpuStatList[4]

    def stop(self):
        echoFile(self.tracingDir+"/tracing_on", "0")

    def clear(self):
        for exp in self.expression:
            probe = exp.split()[0].split(':')[1]
            enableKprobe = self.kprobeDir+"/"+probe+"/enable"
            if os.path.exists(enableKprobe):
                echoFile(enableKprobe, "0")
                echoFileAppend(self.kprobeEvent, '-:%s' % probe)

    def showJson(self, stat, totalTimeout, gloabIowait):
        top = 0
        statJsonStr = '{"time":"", "global iowait":0,"iowait":[]}'
        iowaitStatDicts = json.loads(statJsonStr, object_pairs_hook=OrderedDict)
        iowaitStatDicts['time'] = time.strftime('%Y/%m/%d %H:%M:%S', time.localtime())
        iowaitStatDicts['global iowait'] = gloabIowait
        for pid, item in stat.items():
            if item["timeout"] == 0:
                continue
            if top >= self.top:
                break
            top += 1
            iowait = str(round(item["timeout"] / totalTimeout * gloabIowait, 2))
            item["timeout"] = str(round(item["timeout"]*1000, 3))
            reason = ''
            maxCnt = 0
            for key, val in item['reason'].items():
                if 'balance_dirty' in key:
                    reason = 'Too many dirty pages'
                    break
                elif 'blk_mq_get_tag' in key:
                    reason = 'Device queue full'
                    break
                elif 'get_request' in key:
                    reason = 'Ioscheduler queue full'
                    break
                else:
                    if val > maxCnt:
                        reason = 'Unkown[stacktrace:'+key.replace('<-', '->')+']'
                        maxCnt = val
            iowaitStatJsonStr = '{"comm":"","pid":0,"tgid":0,"timeout":0,"iowait":0,"reason":0}'
            iowaitStatDict = json.loads(
                iowaitStatJsonStr, object_pairs_hook=OrderedDict)
            iowaitStatDict["comm"] = item["comm"]
            iowaitStatDict["pid"] = pid
            iowaitStatDict["tgid"] = item["tgid"]
            iowaitStatDict["timeout"] = item["timeout"]
            iowaitStatDict["iowait"] = iowait
            iowaitStatDict["reason"] = reason
            iowaitStatDicts["iowait"].append(iowaitStatDict)
        if len(iowaitStatDicts["iowait"]) > 0:
            print(json.dumps(iowaitStatDicts))

    def show(self):
        top = 0
        totalTimeout = 0
        stat = {}
        secs = self.cycle
        traceText = []
        commArgs = self.ftracePaserCommArgs

        with open("/proc/stat") as fStat:
            statList = map(long, fStat.readline().split()[1:])
        gloabIowait = float(format(
            (statList[4]-self.cpuStatIowait['iowait'])*100.0 /
            (sum(statList)-self.cpuStatIowait['sum']), '.2f'))
        if gloabIowait < self.iowait_thresh:
            return

        with open(self.tracingDir+"/trace") as f:
            traceLoglist = list(filter(lambda x: any(e in x for e in self.kprobe), f.readlines()))
            traceText = traceLoglist

        # jbd2/vda2-8-605   [001] .... 38890020.539912: p_io_schedule_0: (io_schedule+0x0/0x40)
        # jbd2/vda2-8-605   [002] d... 38890020.540633: r_io_schedule_0: (bit_wait_io+0xd/0x50 <- io_schedule)
        # <...>-130620 [002] .... 38891029.116442: p_io_schedule_timeout_0: (io_schedule_timeout+0x0/0x40)
        # <...>-130620 [002] d... 38891029.123657: r_io_schedule_timeout_0: (balance_dirty_pages+0x270/0xc60 <- io_schedule_timeout)
        for entry in traceText:
            matchObj = re.match(r'(.*) \[([^\[\]]*)\] (.*) (.*): (.*): (.*)'+
                commArgs+'\n', entry)
            if matchObj is None:
                continue
            commInfo = matchObj.group(1).rsplit('-', 1)
            pid = commInfo[1].strip()
            if self.pid is not None and pid != self.pid:
                continue
            if bool(stat.has_key(pid)) != True:
                if len(commArgs):
                    comm = matchObj.group(7).strip("\"")
                else:
                    comm = commInfo[0].lstrip()
                comm = fixComm(comm, pid)
                if '..' in comm:
                    continue
                stat.setdefault(pid, 
                    {"comm": comm, "tgid": getTgid(pid), 
                    "timeout": 0, "reason": {}, "entry": []})
            stat[pid]["entry"].append({
                'time':matchObj.group(4),
                'point':matchObj.group(5),
                'trace':matchObj.group(6)})
        
        if stat:
            for key,item in stat.items():
                item["entry"] = sorted(item["entry"], key=lambda e: float(e["time"]), reverse=False)
                count = 0
                startT = 0
                for entry in item["entry"]:
                    count += 1
                    if (count % 2 != 0 and 'p_' not in entry['point']) or \
                        (count % 2 == 0 and 'r_' not in entry['point']):
                        count = 0
                        startT = 0
                        continue

                    if count % 2 != 0:
                        startT = float(entry['time'])
                        continue

                    if startT > 0 and float(entry['time']) > startT:
                        if re.split('[(,+]', entry['trace'])[1] in re.split('[-,)]', entry['trace'])[1]:
                            count = 0
                            startT = 0
                            continue
                        item['timeout'] += (float(entry['time']) - startT)
                        totalTimeout += (float(entry['time']) - startT)
                        startT = 0
                        if entry['trace'] not in item['reason'].keys():
                            item['reason'].setdefault(entry['trace'], 0)
                        item['reason'][entry['trace']] += 1

        if self.json == False:
            head = str(time.strftime('%Y/%m/%d %H:%M:%S', time.localtime()))+' -> global iowait%: '+str(gloabIowait)
            print head

        if stat:
            stat = OrderedDict(sorted(stat.items(), key=lambda e: e[1]["timeout"], reverse=True))
        if self.json == True:
            self.showJson(stat, totalTimeout, gloabIowait)
            return

        print("%-32s%-8s%-8s%-16s%-12s%s" % ("comm", "tgid", "pid", "waitio(ms)", "iowait(%)", "reasons"))
        for pid, item in stat.items():
            if item["timeout"] == 0:
                continue
            if top >= self.top:
                break
            top += 1
            iowait = str(round(item["timeout"] / totalTimeout * gloabIowait, 2))
            item["timeout"] = str(round(item["timeout"]*1000, 3))
            reason = ''
            maxCnt = 0
            for key, val in item['reason'].items():
                if 'balance_dirty' in key:
                    reason = 'Too many dirty pages'
                    break
                elif 'blk_mq_get_tag' in key:
                    reason = 'Device queue full'
                    break
                elif 'get_request' in key:
                    reason = 'Ioscheduler queue full'
                    break
                else:
                    if val > maxCnt:
                        reason = 'Unkown[stacktrace:'+key.replace('<-', '->')+']'
                        maxCnt = val
            print("%-32s%-8s%-8s%-16s%-12s%s"
                  % (item["comm"], item["tgid"], pid, item["timeout"], iowait, str(reason)))
        print("")

    def entry(self, interval):
        self.start()
        time.sleep(float(interval))
        self.stop()
        self.show()

def main():
    if os.geteuid() != 0:
        print "This program must be run as root. Aborting."
        sys.exit(0)
    examples = """e.g.
  ./iowaitstat.py
			Report iowait for tasks
  ./iowaitstat.py -c 1
			Report iowait for tasks per secs
  ./iowaitstat.py -p [PID] -c 1
			Report iowait for task with [PID] per 1secs
	"""
    parser = argparse.ArgumentParser(
        description="Report iowait for tasks.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=examples)
    parser.add_argument('-p', '--pid', help='Specify the process id.')
    parser.add_argument('-T', '--Timeout',
                        help='Specify the timeout for program exit(secs).')
    parser.add_argument(
        '-t', '--top', help='Report the TopN with the largest iowait.')
    parser.add_argument('-c', '--cycle', help='Specify refresh cycle(secs).')
    parser.add_argument('-j', '--json', action='store_true',
                        help='Specify the json-format output.')
    parser.add_argument('-w','--iowait_thresh', help='Specify the iowait-thresh to report.')
    args = parser.parse_args()

    pid = int(args.pid) if args.pid else None
    secs = int(args.cycle) if args.cycle is not None else 0
    signal.signal(signal.SIGINT, signal_exit_handler)
    signal.signal(signal.SIGHUP, signal_exit_handler)
    signal.signal(signal.SIGTERM, signal_exit_handler)
    if args.Timeout is not None:
        timeoutSec = args.Timeout if args.Timeout > 0 else 10
        signal.signal(signal.SIGALRM, signal_exit_handler)
        signal.alarm(int(timeoutSec))
        secs = 1
    loop = True if secs > 0 else False
    c = iowaitClass(pid, secs, args.top, args.json, args.iowait_thresh)
    c.config()
    interval = secs if loop == True else 1
    while global_stop != True:
        c.entry(interval)
        if loop == False:
            break
    c.clear()


if __name__ == "__main__":
    main()
