#!/usr/bin/python
# -*- coding: utf-8 -*-

import os
import sys
import signal
import string
import argparse
from collections import OrderedDict
from iostatClass import iostatClass
from fsstatClass import fsstatClass

global_stop = False
def signal_exit_handler(signum, frame):
	global global_stop
	global_stop = True

def main():
	if os.geteuid() != 0:
		print "This program must be run as root. Aborting."
		sys.exit(0)
	examples = """e.g.
  ./iofsstat.py -d vda -c 1
			Report IO statistic for vda per 1secs
  ./iofsstat.py -d vda1 --fs -c 1
			Report fs IO statistic for vda1 per 1secs
	"""
	parser = argparse.ArgumentParser(
		description="Report IO statistic for partitions.",
		formatter_class=argparse.RawDescriptionHelpFormatter,
		epilog=examples)
	parser.add_argument('-T','--Timeout', help='Specify the timeout for program exit(secs).')
	parser.add_argument('-t','--top', help='Report the TopN with the largest IO resources.')
	parser.add_argument('-u','--util_thresh', help='Specify the util-thresh to report.')
	parser.add_argument('-b','--bw_thresh', help='Specify the BW-thresh to report.')
	parser.add_argument('-i','--iops_thresh', help='Specify the IOPS-thresh to report.')
	parser.add_argument('-c','--cycle', help='Specify refresh cycle(secs).')
	parser.add_argument('-d','--device', help='Specify the disk name.')
	parser.add_argument('-p','--pid', help='Specify the process id.')
	parser.add_argument('-j','--json', action='store_true',
				help='Specify the json-format output.')
	parser.add_argument('-f','--fs', action='store_true',
			    help='Report filesystem statistic for partitions.')
	args = parser.parse_args()

	secs = int(args.cycle) if args.cycle is not None else 0
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
	loop = True if secs > 0 else False
	if args.fs:
		c = fsstatClass(devname, pid, args.util_thresh, secs, args.bw_thresh,
			 args.top, args.json)
	else:
		c = iostatClass(devname, pid, args.util_thresh, secs, args.iops_thresh,
			 args.bw_thresh, args.top, args.json)
	c.config()
	interval = secs if loop == True else 1
	while global_stop != True:
		c.entry(interval)
		if loop == False:
			break
	c.clear()

if __name__ == "__main__":
	main()
