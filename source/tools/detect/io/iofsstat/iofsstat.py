# -*- coding: utf-8 -*-

import os
import sys
import signal
import string
import argparse
import time
import re
from collections import OrderedDict

def execCmd(cmd):
	r = os.popen(cmd+" 2>/dev/null")
	text = r.read()
	r.close()
	return text

global_stop = False
def signal_exit_handler(signum, frame):
	global global_stop
	global_stop = True

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
	devText = execCmd("cat /sys/class/block/" + devname + "/dev")
	dev = devText.split(':')
	return ((int(dev[0]) << 20) + int(dev[1]))

def getTgid(pid):
	try:
		with open("/proc/"+str(pid)+"/status") as f:
			return ''.join(re.findall(r'Tgid:(.*)',f.read())).lstrip()
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
			#...
			#cpuset,cpu,cpuacct:/docker/e2afa607d8f13e5b1f89d38ee86d86....
			#memory:/docker/e2afa607d8f13e5b1f89d38ee86.....
			#...
			cid = f.read().split("docker/")
			#cid = e2afa607d8f1
			cid = cid[1][0:12] if len(cid) > 1 and len(cid[1]) > 12 else ""
			if re.match('\A[0-9a-fA-F]+\Z', cid):
				dockerRootPath = execCmd("docker inspect -f '{{.HostRootPath}}' "+cid)\
						 .strip('\n')
		#list the open files of the task
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
			if l.find(mntfname)>-1:
				return l.split()[1]
	except IndexError:
		return fsmountInfo[0].split()[1]

def getFullName(fileInfoDict):
	filename = getFullNameFromProcPid(fileInfoDict['pid'], fileInfoDict['ino'])
	if filename == '-':
		mntfname=fileInfoDict['mntfname']
		fsmountInfo=fileInfoDict['fsmountinfo']
		bfname=fileInfoDict['bfname']
		d1fname=fileInfoDict['d1fname']
		d2fname=fileInfoDict['d2fname']
		d3fname=fileInfoDict['d3fname']
		if len(fsmountInfo) != 0:
			mntdir=getMntPath(mntfname,fsmountInfo)
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
	    if ' [' in res:
		res = res.split(" [", 1)[0]
	    if res == name:
		return True
	return False

class diskstatClass(object):
	def __init__(self, devname):
		self.devname = devname
		self.fieldDicts = OrderedDict()
		self.diskInfoDicts = {}
		self.f = open("/proc/diskstats")

	def getDevNameByDevt(self, devt):
		return self.diskInfoDicts[str(devt)]

	def start(self):
		fieldDicts = self.fieldDicts
		diskInfoDicts = self.diskInfoDicts
		self.f.seek(0)
		for stat in self.f.readlines():
			stat = stat.split()
			if self.devname is not None and self.devname not in stat[2] and \
				stat[2] not in self.devname:
				continue

			field = {\
				'1':[0,0], '3':[0,0], '4':[0,0],\
				'5':[0,0], '7':[0,0], '8':[0,0],\
				'10':[0,0], '11':[0,0]}
			for idx,value in field.items():
				value[0] = long(stat[int(idx)+2])
			if stat[2] not in fieldDicts.keys():
				fieldDicts.setdefault(stat[2], field)
				diskInfoDicts.setdefault(str((int(stat[0])<<20)+int(stat[1])), stat[2])
			else:
				fieldDicts[stat[2]].update(field)

	def stop(self):
		fieldDicts = self.fieldDicts
		self.f.seek(0)
		for stat in self.f.readlines():
			stat = stat.split()
			if self.devname is not None and self.devname not in stat[2] and \
				stat[2] not in self.devname:
				continue
			for idx,value in fieldDicts[stat[2]].items():
				value[1] = long(stat[int(idx)+2])

	def show(self, secs):
		fieldDicts = self.fieldDicts

		prefix = self.devname if self.devname is not None else 'disk'
		print('%-20s%-8s%-8s%-12s%-12s%-8s%-8s%-8s%-8s' %\
			(("device-stat:"),"r_iops","w_iops","r_bps",\
			"w_bps","wait","r_wait","w_wait","util%"))
		for devname,field in fieldDicts.items():
			if self.devname is not None and devname != self.devname:
				continue
			r_iops = round((field['1'][1]-field['1'][0]) / secs, 1)
			w_iops = round((field['5'][1]-field['5'][0]) / secs, 1)
			r_bps = (field['3'][1]-field['3'][0]) / secs * 512
			w_bps = (field['7'][1]-field['7'][0]) / secs * 512
			r_ticks = field['4'][1]-field['4'][0]
			w_ticks = field['8'][1]-field['8'][0]
			wait = round((r_ticks+w_ticks)/(r_iops+w_iops), 2) if (r_iops+w_iops) else 0
			r_wait = round(r_ticks / r_iops, 2) if r_iops  else 0
			w_wait = round(w_ticks / w_iops, 2) if w_iops  else 0
			util = round((field['10'][1]-field['10'][0])*100.0/(secs*1000),2)
			util = util if util <= 100 else 100.0
			print('%-20s%-8s%-8s%-12s%-12s%-8s%-8s%-8s%-8s' %\
				(devname,str(r_iops),str(w_iops),humConvert(r_bps, True),\
				humConvert(w_bps, True),str(wait),str(r_wait),str(w_wait),\
				str(util)))
		print("")

	def clear(self):
		self.f.close()

class iostatClass(diskstatClass):
	def __init__(self, devname, pid):
		super(iostatClass, self).__init__(devname)
		self.pid = pid
		self.devname = devname
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

	def show(self, secs):
		stat = {}
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
			iotype = oneIO[-5-comm.count(' ')]
			sectors = oneIO[-2-comm.count(' ')]
			if bool(stat.has_key(comm)) != True:
				stat.setdefault(comm, \
					{"pid":pid, "iops_rd":0,\
					 "iops_wr":0, "bps_rd":0,\
					 "bps_wr":0, "flushIO":0,\
					 "device":super(iostatClass, self).getDevNameByDevt(dev)})
			if 'R' in iotype:
				stat[comm]["iops_rd"] += 1
				stat[comm]["bps_rd"] += (int(sectors) * 512)
			if 'W' in iotype:
				stat[comm]["iops_wr"] += 1
				stat[comm]["bps_wr"] += (int(sectors) * 512)
			if 'F' in iotype:
				stat[comm]["flushIO"] += 1

		super(iostatClass, self).show(secs)
		print('%-20s%-8s%-12s%-16s%-12s%-12s%s' %\
		      ("comm","pid","iops_rd","bps_rd","iops_wr","bps_wr","device"))
		if stat:
			stat = OrderedDict(sorted(stat.items(),\
				key=lambda e:(e[1]["iops_rd"] + e[1]["iops_wr"]),\
				reverse=True))
		for key,item in stat.items():
			if (item["iops_rd"] + item["iops_wr"]) == 0:
				continue
			item["iops_rd"] /= secs
			item["iops_wr"] /= secs
			item["bps_rd"] = humConvert(item["bps_rd"]/secs, True)
			item["bps_wr"] = humConvert(item["bps_wr"]/secs, True)
			print('%-20s%-8s%-12s%-16s%-12s%-12s%s' %\
			      (key,str(item["pid"]),str(item["iops_rd"]),\
			      item["bps_rd"],str(item["iops_wr"]),item["bps_wr"],item["device"]))

	def entry(self, secs):
		loop = True if secs > 0 else False
		global global_stop
		self.config()
		while global_stop != True:
			secs = secs if loop == True else 1
			print(time.strftime('%Y/%m/%d %H:%M:%S', time.localtime()))
			self.start()
			time.sleep(float(secs))
			self.stop()
			self.show(secs)
			print("")
			if loop == False:
				break
		self.clear()

class fsstatClass(diskstatClass):
	def __init__(self, devname, pid):
		super(fsstatClass, self).__init__(devname)
		self.expression = []
		self.pid = pid
		self.devname = devname
		self.devt = getDevt(self.devname) if devname is not None else -1
		tracingBaseDir = "/sys/kernel/debug/tracing"
		self.kprobeEvent = tracingBaseDir+"/kprobe_events"
		self.tracingDir = tracingBaseDir+'/instances/iofsstat'
		self.kprobeDir = self.tracingDir+"/events/kprobes"
		self.kprobe = []
		self.inputExp = []
		version = execCmd('uname -r').split('.')

		offflip = '0x0' if int(version[0]) > 3 or \
			(int(version[0]) == 3 and int(version[1]) > 10) else '0x8'
		offlen = '0x10' if int(version[0]) > 3 else '0x18'
		arch = execCmd('lscpu | grep Architecture').split(":", 1)[1].strip()
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
		# based on surftrace kprobe
		# more details see surftrace project: https://github.com/aliyun/surftrace
		kprobeArgs = 'dev=+0x10(+0x28(+0x20(%s))):u32 '\
		'inode_num=+0x40(+0x20(%s)):u64 len=%s:u64 '\
		'mntfname=+0x0(+0x28(+0x0(+0x10(%s)))):string '\
		'bfname=+0x0(+0x28(+0x18(%s))):string '\
		'd1fname=+0x0(+0x28(+0x18(+0x18(%s)))):string '\
		'd2fname=+0x0(+0x28(+0x18(+0x18(+0x18(%s))))):string '\
		'd3fname=+0x0(+0x28(+0x18(+0x18(+0x18(+0x18(%s)))))):string ' % \
		(argv0,argv0,argv1,argv0,argv0,argv0,argv0,argv0)
		devList = []
		if self.devname is not None:
			devList.append('/dev/'+self.devname)
		else:
			sysfsBlockDirList = os.listdir("/sys/block")
			for dev in sysfsBlockDirList:
				devList.append('/dev/'+dev)
		with open("/proc/mounts") as f:
			self.fsmountInfo = list(filter(lambda x: any(e in x for e in devList), f.readlines()))

		for entry in self.fsmountInfo:
			fstype = entry.split()[2]
			kprobe = fstype+"_file_write_iter"
			if kprobe in self.kprobe:
				continue
			if supportKprobe(kprobe):
				writeKprobe = 'p '+kprobe+' '+kprobeArgs
			elif supportKprobe(fstype+"_file_write"):
				kprobe = fstype+"_file_write"
				writeKprobe = 'p '+kprobe+' '+kprobeArgs
			else:
				print("not available write kprobe")
				sys.exit(0)
			self.kprobe.append(kprobe)
			self.inputExp.append(writeKprobe)

			kprobe = fstype+"_file_read_iter"
			if supportKprobe(kprobe):
				readKprobe = 'p '+kprobe+' '+kprobeArgs
			elif supportKprobe(fstype+"_file_read"):
				kprobe = fstype+"_file_read"
				readKprobe = 'p '+kprobe+' '+kprobeArgs
			elif supportKprobe("generic_file_aio_read"):
				kprobe = "generic_file_aio_read"
				readKprobe = 'p '+kprobe+' '+kprobeArgs
			else:
				print("not available read kprobe")
				sys.exit(0)
			self.kprobe.append(kprobe)
			self.inputExp.append(readKprobe)

			self.expression=self.inputExp
		self.outlogFormatBase = 10

	def config(self):
		devt = self.devt
		if not os.path.exists(self.tracingDir):
			os.mkdir(self.tracingDir)
		for exp in self.expression:
			echoFileAppend(self.kprobeEvent, exp)
			probe='p_'+exp.split()[1]+'_0'
			if devt > 0:
				filterKprobe=self.kprobeDir+"/"+probe+"/filter"
				echoFile(filterKprobe, "dev=="+str(devt))
			enableKprobe=self.kprobeDir+"/"+probe+"/enable"
			echoFile(enableKprobe, "1")
			fmt=execCmd("grep print "+self.kprobeDir+"/"+probe+"/format")
			matchObj=re.match(r'(.*) dev=(.*) inode_num=(.*)', fmt)
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
			probe='p_'+exp.split()[1]+'_0'
			enableKprobe=self.kprobeDir+"/"+probe+"/enable"
			echoFile(enableKprobe, "0")
			if self.devt > 0:
				filterKprobe=self.kprobeDir+"/"+probe+"/filter"
				echoFile(filterKprobe, "0")
			echoFileAppend(self.kprobeEvent, '-:%s'%probe)
		super(fsstatClass, self).clear()

	def show(self, secs):
		stat = {}
		with open(self.tracingDir+"/trace") as f:
			traceText = list(filter(\
				lambda x: any(e in x for e in self.kprobe),\
				f.readlines()))

		#pool-1-thread-2-5029  [002] .... 5293018.252338: p_ext4_file_write_iter_0:\
		# (ext4_file_write_iter+0x0/0x6d0 [ext4]) dev=265289729 inode_num=530392 len=38
		#...
		for entry in traceText:
			matchObj = re.match(r'(.*) \[([^\[\]]*)\] (.*) dev=(.*) '+\
				'inode_num=(.*) len=(.*) mntfname=(.*) '+\
				'bfname=(.*) d1fname=(.*) d2fname=(.*) d3fname=(.*)\n', entry)
			#print(entry)
			if matchObj is not None:
				commInfo = matchObj.group(1).rsplit('-', 1)
			else:
				continue
			pid = commInfo[1].strip()
			if self.pid is not None and pid != self.pid:
				continue
			dev = int(matchObj.group(4),self.outlogFormatBase)
			if self.devt > 0 and self.devt != dev:
				continue
			ino = int(matchObj.group(5),self.outlogFormatBase)
			if bool(stat.has_key(ino)) != True:
				comm = fixComm(commInfo[0].lstrip(), pid)
				if '..' in comm:
					continue
				device = super(fsstatClass, self).getDevNameByDevt(dev)
				fsmountinfo = []
				for f in self.fsmountInfo:
					if ('/dev/'+device) in f:
						fsmountinfo.append(f)

				fileInfoDict = {\
					'device':device,\
					'mntfname':matchObj.group(7).strip("\""),\
					'bfname':matchObj.group(8).strip("\""),\
					'd1fname':matchObj.group(9).strip("\""),
					'd2fname':matchObj.group(10).strip("\""),\
					'd3fname':matchObj.group(11).strip("\""),\
					'fsmountinfo':fsmountinfo,\
					'ino':ino,'pid':pid}
				stat.setdefault(ino,\
				    {"comm":comm,"tgid":getTgid(pid),"pid":pid,\
				    "cnt_wr":0,"bw_wr":0,"cnt_rd":0,"bw_rd":0,\
				    "device":device,\
					"file":getFullName(fileInfoDict)})
			size = int(matchObj.group(6),self.outlogFormatBase)
			if 'write' in entry:
				stat[ino]["cnt_wr"] += 1
				stat[ino]["bw_wr"] += int(size)
			if 'read' in entry:
				stat[ino]["cnt_rd"] += 1
				stat[ino]["bw_rd"] += int(size)

		super(fsstatClass, self).show(secs)
		print("%-20s%-8s%-8s%-8s%-12s%-8s%-12s%-12s%-12s%s"\
		      %("comm","tgid","pid","cnt_rd","bw_rd",\
		      "cnt_wr","bw_wr","inode","device","filepath"))
		if stat:
			stat = OrderedDict(sorted(stat.items(),\
				key=lambda e:(e[1]["bw_wr"]+e[1]["bw_rd"]),reverse=True))
		for key,item in stat.items():
			if (item["cnt_wr"] + item["cnt_rd"]) == 0:
				continue
			item["cnt_wr"]/=secs
			item["bw_wr"]=humConvert(item["bw_wr"]/secs,True)
			item["cnt_rd"]/=secs
			item["bw_rd"]=humConvert(item["bw_rd"]/secs,True)
			print("%-20s%-8s%-8s%-8d%-12s%-8d%-12s%-12s%-12s%s"\
			      %(item["comm"],item["tgid"],item["pid"],\
			      item["cnt_rd"],item["bw_rd"],item["cnt_wr"],\
			      item["bw_wr"],key,item["device"],item["file"]))

	def entry(self, secs):
		loop = True if secs > 0 else False
		global global_stop
		self.config()
		while global_stop != True:
			secs = secs if loop == True else 1
			print(time.strftime('%Y/%m/%d %H:%M:%S', time.localtime()))
			self.start()
			time.sleep(float(secs))
			self.stop()
			self.show(secs)
			print("")
			if loop == False:
				break
		self.clear()

def main():
	if os.geteuid() != 0:
		print "This program must be run as root. Aborting."
		sys.exit(0)
	examples = """e.g.
  ./iofsstat.py -i 1
			Report IO statistic for all disk per 1secs
  ./iofsstat.py -d vda -i 1
			Report IO statistic for vda per 1secs
  ./iofsstat.py --fs -i 1
			Report fs IO statistic for all partitions per 1secs
  ./iofsstat.py -d vda1 --fs -i 1
			Report fs IO statistic for vda1 per 1secs
	"""
	parser = argparse.ArgumentParser(
		description="Report IO statistic for partitions.",
		formatter_class=argparse.RawDescriptionHelpFormatter,
		epilog=examples)
	parser.add_argument('-T','--Timeout', help='Specify the timeout for program exit(secs).')
	parser.add_argument('-i','--interval', help='Specify refresh interval(secs).')
	parser.add_argument('-d','--device', help='Specify the disk or partition name.')
	parser.add_argument('-p','--pid', help='Specify the process id.')
	parser.add_argument('-f','--fs', action='store_true',\
			    help='Report filesystem statistic for partitions.')
	args = parser.parse_args()

	secs = int(args.interval) if args.interval is not None else 0
	devname = args.device
	pid = int(args.pid) if args.pid else None
	signal.signal(signal.SIGINT, signal_exit_handler)
	signal.signal(signal.SIGHUP, signal_exit_handler)
	signal.signal(signal.SIGTERM, signal_exit_handler)
	if args.Timeout is not None:
		timeoutSec = args.Timeout if args.Timeout > 0 else 10
		signal.signal(signal.SIGALRM, signal_exit_handler)
		signal.alarm(int(timeoutSec))
		secs = 1
	if args.fs:
		fsstatClass(devname, pid).entry(secs)
	else:
		iostatClass(devname, pid).entry(secs)

if __name__ == "__main__":
	main()

