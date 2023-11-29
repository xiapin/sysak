#!/usr/bin/python
# -*- coding: utf-8 -*-

import os
import sys
import signal
import string
import argparse
from iotracingClass import iotracingClass

global_stop = False
def signal_exit_handler(signum, frame):
    global global_stop
    global_stop = True

def main():
    if os.geteuid() != 0:
        print ("This program must be run as root. Aborting.")
        sys.exit(0)
    examples = """e.g.
  ./iolatency.py -t 10 -t 10 -d vda
            Report io delay over 10ms for vda 10s
    """
    parser = argparse.ArgumentParser(
        description="Report IO delay for disk.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=examples)
    parser.add_argument('-T','--Timeout', help='Specify the timeout for program exit(secs).')
    parser.add_argument('-t','--thresh', help='Specify the delay-thresh to report.')
    parser.add_argument('-d','--device', help='Specify the disk name.')
    parser.add_argument('-f','--log', help='Specify the json-format output.')
    args = parser.parse_args()

    signal.signal(signal.SIGINT, signal_exit_handler)
    signal.signal(signal.SIGHUP, signal_exit_handler)
    signal.signal(signal.SIGTERM, signal_exit_handler)
    if args.Timeout is not None:
        timeoutSec = args.Timeout if int(args.Timeout) > 0 else 10
        signal.signal(signal.SIGALRM, signal_exit_handler)
        signal.alarm(int(timeoutSec))
    c = iotracingClass(args.device, args.thresh, args.log)
    c.config()

    while global_stop != True:
        c.entry(1)
    c.clear()

if __name__ == "__main__":
    main()
