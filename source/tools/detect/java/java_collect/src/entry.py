# -*- coding: utf-8 -*-
"""
-------------------------------------------------
   File Name：     entry
   Description :
   Author :       liaozhaoyan
   date：          2023/7/23
-------------------------------------------------
   Change Activity:
                   2023/7/23:
-------------------------------------------------
"""
__author__ = 'liaozhaoyan'

import argparse
from jspy import Cjspy


def setup_args():
    examples = """e.g.
        java_collect -d 10
        java_collect -d 10 -p 1346
        java_collect -d 10 -t 3
        java_collect -d 5 -p 47184,1234 -b
        """
    parser = argparse.ArgumentParser(
        description="collect cpu flamegraph for java processes",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=examples)
    parser.add_argument('-d', '--dur', dest='duration', type=int, default=5, help='duration, default: 5 seconds')
    parser.add_argument('-t', '--top', dest='top', type=int, default=-1, 
                        help='Flame graphs for the top java processes. Default: 1, Max: 20')
    parser.add_argument('-p', '--pid', dest='pid', type=str, default="", help='specified pid')
    parser.add_argument('-b', '--bpf', dest='bpf', action='store_true', default=False,
                        help='eBPF oncpu sampling profiler. Default: True')
    parser.add_argument('--oss', dest='oss', action='store_true', default=True,
                        help='put on oss.')
    parser.add_argument('--zip', dest='oss', action='store_false', default=True,
                        help='pack to zip.')
    return parser.parse_args()


def check():
    args = setup_args()
    if args.top == -1 and args.pid == "":
        raise ValueError("should set top N or pid.")
    if args.top != -1 and args.pid != "":
        raise ValueError("should not both set top N and pid.")
    return args
    

if __name__ == "__main__":
    s = Cjspy(check())
    s.diag()
    pass
