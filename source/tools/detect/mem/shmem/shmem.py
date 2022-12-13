#!/usr/bin/python2
# -*- coding: UTF-8 -*-

import re
import os.path
import getopt
import sys
import time


result_dir = "/var/log/sysak/shmem/"
old_log = result_dir + "shmem_old_log"
new_log = result_dir + "shmem_new_log"
result_log = result_dir + "result_log"

default_time = 5

def exectue_cmd(command):
    command=command.replace("\n", "")
    command_fd = os.popen(command, "r")
    ret = command_fd.read()
    command_fd.close()
    return ret


def shmem_check(checktime):
    shmemkey_dict = {}
    shmeminc_dict = {}
    newkey_dict = {}
    oldkey_dict = {}

    if not os.path.exists(result_dir):
        os.mkdir(result_dir)

    if os.path.exists(result_log):
        os.remove(result_log)
    if os.path.exists(new_log):
        os.remove(new_log)
    if os.path.exists(old_log):
        os.remove(old_log)

    command_old = "ipcs -m >" + old_log
    exectue_cmd(command_old)   
    time.sleep(checktime)
    command_new = "ipcs -m >" + new_log
    exectue_cmd(command_new)

    if os.path.exists(old_log):
        old_fd = open(old_log,"r")
        for line in old_fd.readlines():
            pattern1 = re.search( r'0x(\w+)(.*)', line, re.I)
            if pattern1:
                sub = line.split()
                old_size = sub[4]
                oldkey_dict[pattern1.group(1)] = old_size
        old_fd.close
    if os.path.exists(new_log):
        new_fd = open(new_log,"r")
        for line in new_fd.readlines():
            pattern2 = re.search( r'0x(\w+)(.*)', line, re.I)
            if pattern2:
                sub = line.split()
                new_size = sub[4]
                newkey_dict[pattern2.group(1)] = new_size
        new_fd.close
    for key,value in newkey_dict.items():
        if value > oldkey_dict[key]:
            shmemkey_dict[key] = value
            shmeminc_dict[key] = int(value) - int(oldkey_dict[key])

    pid_path='/proc'
    pid_folders= os.listdir(pid_path)   
    for file in pid_folders:
        pid_smaps = "NULL"
        pid_comm = "NULL"
        name = "NULL" 
        if file.isdigit():
            pid_smaps = pid_path + "/" + file + "/maps"
            pid_comm = pid_path + "/" + file + "/comm"
            if os.path.exists(pid_comm):
                comm_fd = open(pid_comm,"r")
                line_tmp = comm_fd.readlines()
                name = line_tmp[0].split('\n')[0]
                comm_fd.close
            if os.path.exists(pid_smaps):
                smaps_fd = open(pid_smaps,"r")
                for line in smaps_fd.readlines():
                    for key,value in shmemkey_dict.items():
                        if line.find(key) != -1:
                            if not os.path.exists(result_log):
                                rseult_fd = open(result_log,"a")
                                print("%s%s%s%s%s" % ("shmem key".ljust(15),"pid".ljust(15),"name".ljust(20),"total size(bytes)".ljust(20),"increase(bytes)".ljust(15)))
                                rseult_fd.write("shmem key".ljust(15)+"pid".ljust(15)+"name".ljust(20)+"total size(bytes)".ljust(20)+"increase(bytes)".ljust(15)+"\n")
                                rseult_fd.close
                            print(key.ljust(15)+file.ljust(15)+name.ljust(20)+value.ljust(20)+str(shmeminc_dict[key]))
                            rseult_fd = open(result_log,"a")
                            rseult_fd.write(key.ljust(15)+file.ljust(15)+name.ljust(20)+value.ljust(20)+str(shmeminc_dict[key])+"\n")
                            rseult_fd.close
                smaps_fd.close

def usage():
    print ('shmem: shmem leak check tool')
    print ('Usage: shmem <option> [<args>]')
    print ('  -h                 help information')
    print ('  -t                 specify the monitor period(s), default=5s')
    print ('example:')
    print ('shmem.py -t 5')
    return


opts,args = getopt.getopt(sys.argv[1:],'-h-t:')

def main():
    for opt_name,opt_value in opts:
        if opt_name in ('-h'):
            usage()
            sys.exit()
        if opt_name in ('-t'):
            if opt_value:
                time = int(opt_value)
                shmem_check(time)
            else:
                time = default_time
            sys.exit()
    usage()

if __name__ == '__main__':
    main()