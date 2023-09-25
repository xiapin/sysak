# -*- coding: utf-8 -*-
#!/root/anaconda3/envs/python310
import argparse
from cpu import cpu_data_show
from unity_total import unity_total
from unity_io import io_data_show
from tcp import tcp_data_show
from pcsw import pcsw_data_show
from traffic import traffic_data_show
from udp import udp_data_show
from paritition import partition_data_show
from mem import mem_data_show
from load import load_data_show


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    
    parser.add_argument('-i','--instance',type=int, help='data spacing')
    parser.add_argument('-t','--time',type=int, help='query mintes old data')
    parser.add_argument('-d','--date',type=int, help='query hours old data')
    parser.add_argument('--cpu', action='store_true', help='CPU share (user, system, interrupt, nice, & idle)')
    parser.add_argument('--mem', action='store_true', help='Physical memory share (active, inactive, cached, free, wired)')
    parser.add_argument('--load', action='store_true', help='System Run Queue and load average')
    parser.add_argument('--traffic', action='store_true', help='Net traffic statistics')
    parser.add_argument('--tcp', action='store_true', help='TCP traffic     (v4)')
    parser.add_argument('--udp', action='store_true', help='UDP traffic     (v4)')
    parser.add_argument('--io', action='store_true', help='Linux I/O performance')
    parser.add_argument('--partition', action='store_true', help='Disk and partition usage')
    parser.add_argument('--pcsw', action='store_true', help='Process (task) creation and context switch')
    args = parser.parse_args()
    minutes = args.time
    if args.cpu:
        cpu_data_show(distance_max=args.instance, minutes=args.time, date=args.date)
    elif args.mem:
        mem_data_show(distance_max=args.instance, minutes=args.time, date=args.date)
    elif args.load:
        load_data_show(distance_max=args.instance, minutes=args.time, date=args.date)
    elif args.traffic:
        traffic_data_show(distance_max=args.instance, minutes=args.time, date=args.date)
    elif args.tcp:
        tcp_data_show(distance_max=args.instance, minutes=args.time, date=args.date)
    elif args.udp:
        udp_data_show(distance_max=args.instance, minutes=args.time, date=args.date)
    elif args.io:
        io_data_show(distance_max=args.instance, minutes=args.time, date=args.date)
    elif args.partition:
        partition_data_show(distance_max=args.instance, minutes=args.time, date=args.date)
    elif args.pcsw:
        pcsw_data_show(distance_max=args.instance, minutes=args.time, date=args.date)
    else:
        unity_total(distance_max=args.instance, minutes=args.time, date=args.date)
