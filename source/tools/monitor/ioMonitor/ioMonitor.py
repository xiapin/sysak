# -*- coding: utf-8 -*-
import argparse
import signal
import os
from ioMonCfgClass import ioMonCfgClass
from ioMonitorClass import ioMonitorClass

setcfg_descripton = """set monitor cfg, like -s \'xxxx=xxx,xxx=xxx\'
iowait, The min cpu-iowait not report exceptions(1~100, default 5).
await, The min partition-await not report exceptions(N ms, default 10).
util, The min partition-util not report exceptions(1~100, default 20).
bps, The min partition-bps not report exceptions(bytes, default 30MB).
iops, The min partition-iops not report exceptions(default 150).
cycle, The cycle of Monitoring data collection(default 500ms).
diagIowait, Disable or enable diagnose while reporting iowait exceptions(default on).
diagIolat, Disable or enable diagnose while reporting latency exceptions(default on).
diagIoburst, Disable or enable diagnose while reporting ioburst exceptions(default on).
diagIohang, Disable or enable diagnose while reporting iohang exceptions(default on).
"""

def getRunArgsFromYaml(yamlF):
    logRootPath = ''
    pipeFile = ''
    with open(yamlF) as f:
        lines = f.readlines()
    for l in lines:
        if not l.startswith('#'):
            if 'proc_path:' in l:
                logRootPath = l.split()[1].strip('\n')
            elif 'outline:' in l:
                pipeFile = lines[lines.index(l) + 1].split()[1].strip('\n')
        if logRootPath and pipeFile:
            break
    if not logRootPath or not pipeFile:
        raise ValueError(
            'Unable to get labels \"proc_path\" and \"outline\" in %s' % yamlF)
    return logRootPath+'/run',pipeFile


def main():
    examples = """e.g.
  ./ioMonitorMain.py -y [yaml_file]
            Start ioMonitor
  ./ioMonitorMain.py -y [yaml_file] --reset_cfg --only_set_cfg
            Only reset cfg to default
  ./ioMonitorMain.py -y [yaml_file] -s 'iowait=10,iops=200,diagIowait=on' --only_set_cfg
            Only set min-iowait&&min-iops and disable iowait diagnose to config.
    """
    parser = argparse.ArgumentParser(
        description="start ioMonitor.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=examples)
    parser.add_argument('-y','--yaml_file', 
        help='Specify the socket pipe for data upload'\
            ' and exception log path')
    parser.add_argument('-s','--set_cfg', help=setcfg_descripton)
    parser.add_argument('-r','--reset_cfg', action='store_true',
                        help='Reset cfg to default')
    parser.add_argument('-o','--only_set_cfg', action='store_true',
                        help='Only set cfg')
    parser.add_argument('-t','--timeout', type=int, default=0, help='Monitoring duration')
    parser.add_argument('-a','--analysis_file', type=str, default=\
        '/var/log/sysak/iosdiag/iodiagnose/iodiagnose.log', help='Store diagnosis result')
    args = parser.parse_args()

    signal.signal(signal.SIGCHLD, signal.SIG_IGN)
    logRootPath,pipeFile = getRunArgsFromYaml(args.yaml_file)
    if args.only_set_cfg:
        if not os.path.exists(logRootPath+'/ioMon/ioMonCfg.json'):
            print("%s" % ("config fail, not found ioMonCfg.json"))
            return
        if args.set_cfg is None and not args.reset_cfg:
            print("%s" % ("--set_cfg or --reset_cfg not found."))
            return
        ioMonCfg = ioMonCfgClass(args.set_cfg, args.reset_cfg, logRootPath)
        ioMonCfg.notifyIoMon()
        return
    
    mode = "monitor"
    if args.timeout < 0:
        args.timeout = 0
    elif args.timeout > 0:
        mode = "diagnose"
    
    if os.path.exists(args.analysis_file):
        os.remove(args.analysis_file)

    ioMonCfg = ioMonCfgClass(args.set_cfg, args.reset_cfg, logRootPath)
    ioMon = ioMonitorClass(logRootPath, ioMonCfg, pipeFile, mode, args.analysis_file)
    ioMon.monitor(args.timeout)

if __name__ == "__main__":
    main()
