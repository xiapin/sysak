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

def echoFile(filename, txt):
	execCmd("echo \""+txt+"\" > "+filename)

class iostatClass(diskstatClass):
	def __init__(self, devname, pid, utilThresh, cycle, iopsThresh, bwThresh, top, json):
		super(iostatClass, self).__init__(devname, utilThresh, cycle, json)
		self.pid = pid
		self.devname = devname
		self.top = int(top) if top is not None else 99999999
		self.iopsThresh = int(iopsThresh) if iopsThresh is not None else 0
		self.bwThresh = int(bwThresh) if bwThresh is not None else 0
		self.devt = getDevt(self.devname) if devname is not None else -1
		self.tracingDir="/sys/kernel/debug/tracing/instances/iofsstat"
		self.blkTraceDir=self.tracingDir+"/events/block"

	def config(self):
		devt = self.devt
		if not os.path.exists(self.tracingDir):
			os.mkdir(self.tracingDir)
		if devt > 0:
			echoFile(self.blkTraceDir+"/block_getrq/filter", "dev=="+str(devt))
		echoFile(self.blkTraceDir+"/block_getrq/enable", "1")

	def start(self):
		echoFile(self.tracingDir+"/trace", "")
		echoFile(self.tracingDir+"/tracing_on", "1")
		super(iostatClass, self).start()

	def stop(self):
		echoFile(self.tracingDir+"/tracing_on", "0")
		super(iostatClass, self).stop()

	def clear(self):
		echoFile(self.blkTraceDir+"/block_getrq/enable", "0")
		if self.devt > 0:
			echoFile(self.blkTraceDir+"/block_getrq/filter", "0")
		super(iostatClass, self).clear()

	def showJson(self,stat):
		top = 0
		secs = self.cycle
		statJsonStr = '{\
			"time":"",\
			"iostats":[]}'
		iostatDicts = json.loads(statJsonStr, object_pairs_hook=OrderedDict)
		iostatDicts['time'] = time.strftime('%Y/%m/%d %H:%M:%S', time.localtime())
		for key,item in stat.items():
			if (item["iops_rd"] + item["iops_wr"]) == 0:
				continue
			if top >= self.top:
				break
			top += 1
			item["iops_rd"] /= secs
			item["iops_wr"] /= secs
			item["bps_rd"] = humConvert(item["bps_rd"]/secs, True)
			item["bps_wr"] = humConvert(item["bps_wr"]/secs, True)
			iostatJsonStr = '{\
				"comm":"",\
				"pid":0,\
				"bps_rd":0,\
				"iops_rd":0,\
				"iops_wr":0,\
				"bps_wr":0,\
				"device":0}'
			iostatDict = json.loads(iostatJsonStr, object_pairs_hook=OrderedDict)
			iostatDict["comm"] = key
			for key in ['pid', 'bps_rd', 'iops_rd', 'iops_wr', 'bps_wr', 'device']:
				iostatDict[key] = item[key]
			iostatDicts["iostats"].append(iostatDict)
		if len(iostatDicts["iostats"]) > 0:
			print(json.dumps(iostatDicts))

	def show(self):
		top = 0
		iopsTotal = 0
		bwTotal = 0
		stat = {}
		secs = self.cycle
		with open(self.tracingDir+"/trace") as f:
			traceText = list(filter(lambda x: 'block_getrq' in x, f.readlines()))
		#jbd2/vda1-8-358 ... : block_getrq: 253,0 WS 59098136 + 120 [jbd2/vda1-8]
		for entry in traceText:
			oneIO = entry.split()
			matchObj = re.match(r'(.*) \[([^\[\]]*)\] (.*) \[([^\[\]]*)\]\n',entry)
			comm = matchObj.group(4)
			pid = matchObj.group(1).rsplit('-', 1)[1].strip()
			if self.pid is not None and pid != self.pid:
				continue
			devinfo = oneIO[-6-comm.count(' ')].split(',')
			dev = ((int(devinfo[0]) << 20) + int(devinfo[1]))
			if self.devt > 0 and self.devt != dev:
				continue
			device = super(iostatClass, self).getDevNameByDevt(dev)
			if super(iostatClass, self).notCareDevice(device) == True:
				continue
			iotype = oneIO[-5-comm.count(' ')]
			sectors = oneIO[-2-comm.count(' ')]
			if bool(stat.has_key(comm)) != True:
				stat.setdefault(comm, \
					{"pid":pid, "iops_rd":0,\
					 "iops_wr":0, "bps_rd":0,\
					 "bps_wr":0, "flushIO":0,\
					 "device":device})
			if 'R' in iotype:
				stat[comm]["iops_rd"] += 1
				stat[comm]["bps_rd"] += (int(sectors) * 512)
				bwTotal += (int(sectors) * 512)
				iopsTotal += 1
			if 'W' in iotype:
				stat[comm]["iops_wr"] += 1
				stat[comm]["bps_wr"] += (int(sectors) * 512)
				bwTotal += (int(sectors) * 512)
				iopsTotal += 1
			if 'F' in iotype:
				stat[comm]["flushIO"] += 1

		if super(iostatClass, self).disableShow() == True or \
			((iopsTotal/secs) < self.iopsThresh and \
			(bwTotal/secs) < self.bwThresh):
			return

		if super(iostatClass, self).enableJsonShow() == False:
			print(time.strftime('%Y/%m/%d %H:%M:%S', time.localtime()))
		super(iostatClass, self).show()

		if stat:
			stat = OrderedDict(sorted(stat.items(),\
				key=lambda e:(e[1]["iops_rd"] + e[1]["iops_wr"]),\
				reverse=True))
		if super(iostatClass, self).enableJsonShow() == True:
			self.showJson(stat)
			return

		print('%-20s%-8s%-12s%-16s%-12s%-12s%s' %\
		      ("comm","pid","iops_rd","bps_rd","iops_wr","bps_wr","device"))
		for key,item in stat.items():
			if (item["iops_rd"] + item["iops_wr"]) == 0:
				continue
			if top >= self.top:
				break
			top += 1
			item["iops_rd"] /= secs
			item["iops_wr"] /= secs
			item["bps_rd"] = humConvert(item["bps_rd"]/secs, True)
			item["bps_wr"] = humConvert(item["bps_wr"]/secs, True)
			print('%-20s%-8s%-12s%-16s%-12s%-12s%s' %\
			      (key,str(item["pid"]),str(item["iops_rd"]),\
			      item["bps_rd"],str(item["iops_wr"]),item["bps_wr"],item["device"]))
		print("")

	def entry(self, interval):
		self.start()
		time.sleep(float(interval))
		self.stop()
		self.show()
