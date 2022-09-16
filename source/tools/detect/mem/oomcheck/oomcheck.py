#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# @Author: changjun

from subprocess import *
import os, fcntl, re, sys
from time import sleep
import socket
import time,datetime
import json,base64,hashlib,re
import threading
import sched
import importlib
import json
import argparse
import getopt
import traceback

OOM_REASON_CGROUP = 'cgroup memory limit',
OOM_REASON_PCGROUP = 'parent cgroup memory limit',
OOM_REASON_HOST = 'host memory limit',
OOM_REASON_MEMLEAK = 'host memory limit,may caused by memory leak',
OOM_REASON_NODEMASK = 'mempolicy not allowed process to use all the memory of NUMA system'
OOM_REASON_NODE = 'cpuset cgroup not allowed process to use all the memory of NUMA system',
OOM_REASON_MEMFRAG = 'memory fragment',
OOM_REASON_SYSRQ = 'sysrq',
OOM_REASON_OTHER = 'other'


OOM_BEGIN_KEYWORD = "invoked oom-killer"
OOM_END_KEYWORD = "Killed process"
OOM_END_KEYWORD_4_19 = "reaped process"
OOM_CGROUP_KEYWORD = "Task in /"
OOM_NORMAL_MEM_KEYWORD = "Normal: "
OOM_PID_KEYWORD = "[ pid ]"
WEEK_LIST = ['Mon','Tue','Wed','Thu','Fri','Sat','Sun']
CWEEK_LIST = ['一','二','三','四','五','六','日']
MONTH_LIST = ['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec']
CMONTH_LIST = ['1月','2月','3月','4月','5月','6月','7月','8月','9月','10月','11月','12月']
mems_pattern = re.compile("cpuset=(.*)[ ,]+mems_allowed=([0-9\-\/\,]*)")
node_pattern = re.compile("nodemask=\(?([0-9\-\/\,null]*)\)?")


if sys.version[0] == '2':
    reload(sys)
    sys.setdefaultencoding('utf8')

def set_to_list(setstr):
    setstr = setstr.split(',')
    resset = []
    for line in setstr:
        try:
            line = line.strip()
            if not line:
                continue
            if line[0] == '(' and line[-1] == ')':
                line = line[1:-1]
            if line.find('null') >= 0:
                resset.append(-1)
                break
            if line.find('-') >= 0:
                resset.extend([i for i in range(int(line.split('-')[0]), int(line.split('-')[1])+1)])
            else:
                resset.append(int(line))
        except Exception as err:
            sys.stderr.write("set_to_list loop err {} lines {}\n".format(err, traceback.print_exc()))
            continue
    return resset

def bignum_to_num(ori_num):
    try:
        ret_num = ori_num
        if 'kB' in ori_num:
            ret_num = str(int(ori_num.rstrip('kB')) * 1024)
        elif 'KB' in ori_num:
            ret_num = str(int(ori_num.rstrip('KB')) * 1024)
        elif 'k' in ori_num:
            ret_num = str(int(ori_num.rstrip('k')) * 1024)
        elif 'K' in ori_num:
            ret_num = str(int(ori_num.rstrip('K')) * 1024)
        if 'M' in ori_num:
            ret_num = str(int(ori_num.rstrip('M')) * 1024*1024)
        if 'G' in ori_num:
            ret_num = str(int(ori_num.rstrip('G')) * 1024*1024*1024)
        if 'm' in ori_num:
            ret_num = str(int(ori_num.rstrip('m')) * 1024*1024)
        if 'g' in ori_num:
            ret_num = str(int(ori_num.rstrip('g')) * 1024*1024*1024)
        return ret_num
    except:
        return ori_num

def oom_get_ts(oom_time):
    if oom_time.find(".") == -1:
        return 0
    return float(oom_time)

def oom_get_ymdh(oom_time):
    if oom_time.find(":") == -1:
        return 0
    oom_time = oom_time.split()
    ret_time = ""
    if oom_time[0] in WEEK_LIST:
        ret_time = "%s-%02d-%02d %s"%(oom_time[4],MONTH_LIST.index(oom_time[1])+1,int(oom_time[2]),oom_time[3])
    elif oom_time[0] in CWEEK_LIST:
        ret_time = "%s-%02d-%02d %s"%(oom_time[4],CMONTH_LIST.index(oom_time[1])+1,int(oom_time[2]),oom_time[3])
    return normal_time2ts(ret_time)

def oom_time_to_normal_time(oom_time):
    if len(oom_time.strip()) == 0:
        return 0
    try:
        if oom_time.find(":") != -1:
            return oom_get_ymdh(oom_time)
        elif oom_time.find(".") != -1:
            return oom_get_ts(oom_time)
    except:
        return 0

def normal_time2ts(oom_time):
    if len(oom_time) < 8:
        return 0
    ts = time.strptime(oom_time, "%Y-%m-%d %H:%M:%S")
    return float(time.mktime(ts))

def oomcheck_get_spectime(time, oom_result):
    try:
        delta = 3153600000
        num = oom_result['oom_total_num']
        for i in range(oom_result['oom_total_num']):
            time_oom = oom_result['sub_msg'][i+1]['time']
            if abs(time - time_oom) <= delta:
                delta  = abs(time - time_oom)
                num = i
        return num + 1
    except Exception as err:
        sys.stderr.write("oomcheck_spectime error {}\n".format(err))

def oom_is_node_num(line):
    return "hugepages_size=1048576" in line

def oom_get_mem_allowed(oom_result, line, num):
    gp = mems_pattern.search(line)
    if not gp:
        return
    cpuset = gp.group(1)
    allowed = gp.group(2)
    oom_result['sub_msg'][num]['mems_allowed'] = set_to_list(allowed)
    oom_result['sub_msg'][num]['cpuset'] = cpuset

def oom_is_host_oom(reason):
    return reason == OOM_REASON_HOST

def oom_get_pid(oom_result, line, num):
    if OOM_END_KEYWORD in line:
        split_line = OOM_END_KEYWORD
    else:
        split_line = OOM_END_KEYWORD_4_19
    pid = line.strip().split(split_line)[1].strip().split()[0]
    oom_result['sub_msg'][num]['pid'] = pid

def oom_get_task_mem(oom_result, line, num):
    anon_rss = 0
    file_rss = 0
    shmem_rss = 0
    if line.find('anon-rss') != -1:
        anon_rss = line.strip().split('anon-rss:')[1].split()[0].strip(',')
    if line.find('file-rss') != -1:
        file_rss = line.strip().split('file-rss:')[1].split()[0].strip(',')
    if line.find('shmem-rss') != -1:
        shmem_rss = line.strip().split('shmem-rss:')[1].split()[0].strip(',')
    oom_result['sub_msg'][num]['killed_task_mem'] = (
        int(bignum_to_num(anon_rss)) + int(bignum_to_num(file_rss)) + int(bignum_to_num(shmem_rss)))/1024

def oom_get_host_mem(oom_result, line, num):
    oom_result['sub_msg'][num]['reason'] = OOM_REASON_HOST
    oom_result['sub_msg'][num]['type'] = 'host'
    memory_free = line.strip().split('Normal free:')[1].split()[0]
    memory_low = line.strip().split('low:')[1].split()[0]
    oom_result['sub_msg'][num]['host_free'] = memory_free
    oom_result['sub_msg'][num]['host_low'] = memory_low

def oom_get_cgroup_mem(oom_result, line, num):
    memory_usage = line.strip().split('memory: usage')[1].split()[0].strip(',')
    memory_limit = line.strip().split('limit')[1].split()[0].strip(',')
    oom_result['sub_msg'][num]['cg_usage'] = memory_usage
    oom_result['sub_msg'][num]['cg_limit'] = memory_limit

def oom_get_cgroup_shmem(oom_result, line, num):
    inanon = "0"
    anon = "0"
    rss = '0'
    oom_result['sub_msg'][num]['cg_inanon'] = inanon;
    oom_result['sub_msg'][num]['cg_anon'] = anon;
    oom_result['sub_msg'][num]['cg_rss'] = rss;

    if line.find("inactive_anon:") == -1:
        return
    inanon = line.strip().split("inactive_anon:")[1]
    inanon = inanon.split()[0][:-2]

    anon = line.strip().split("inactive_anon:")[1]
    anon = anon.strip().split("active_anon:")[1]
    anon = anon.split()[0][:-2]

    rss = line.strip().split("rss:")[1]
    rss = rss.split()[0][:-2]

    oom_result['sub_msg'][num]['cg_inanon'] = inanon;
    oom_result['sub_msg'][num]['cg_anon'] = anon;
    oom_result['sub_msg'][num]['cg_rss'] = rss;

def oom_get_cgroup_name(oom_result, line, num):
    is_host = False
    if "limit of host" in line:
        is_host = True
    if is_host == False:
        oom_result['sub_msg'][num]['reason'] = OOM_REASON_CGROUP
        oom_result['sub_msg'][num]['type'] = 'cgroup'
    task_list = line.strip().split("Task in")[1].strip().split()
    cgroup = task_list[0]
    pcgroup = task_list[-1]
    if is_host == False and cgroup != pcgroup:
       #cgroup = pcgroup
       oom_result['sub_msg'][num]['reason'] = OOM_REASON_PCGROUP
    oom_result['sub_msg'][num]['cg_name'] = cgroup

def oom_get_order(oom_result, line, num):
    order = int(line.strip().split("order=")[1].split()[0][:-1])
    oom_result['sub_msg'][num]['order'] = order

def oom_get_nodemask(oom_result, line, num):
    gp = node_pattern.search(line)
    if not gp:
        return
    nodemask = gp.group(1)
    oom_result['sub_msg'][num]['nodemask'] = set_to_list(nodemask)

def oom_set_node_oom(oom_result, num, node_num):
    task_mem_allow = oom_result['sub_msg'][num]['mems_allowed']
    is_host = oom_is_host_oom(oom_result['sub_msg'][num]['reason'])
    if is_host and len(task_mem_allow) != node_num:
            oom_result['sub_msg'][num]['reason'] = OOM_REASON_NODE
            oom_result['sub_msg'][num]['type'] = 'node'

def oom_get_hugepage(oom_result, line, num):
    if line.find('hugepages_total') == -1 or line.find('hugepages_size') == -1:
        return True
    oom = oom_result['sub_msg'][num]
    if 'hugepage' not in oom['meminfo']:
        oom['meminfo']['hugepage'] = 0

    hugetotal = line.split('hugepages_total=')[1]
    hugetotal = int(hugetotal.strip().split()[0])

    hugesize = line.split('hugepages_size=')[1]
    hugesize = int(hugesize.strip()[:-2])
    oom['meminfo']['hugepage'] = oom['meminfo']['hugepage'] + hugetotal*hugesize
    #print("hugetotal: {} size:{}".format(hugetotal, hugesize))

meminfo_pattern = ([re.compile("(active_anon):(\S+) (inactive_anon):(\S+) (isolated_anon):(\S+)")
, re.compile("(active_file):(\S+) (inactive_file):(\S+) (isolated_file):(\S+)")
, re.compile("(unevictable):(\S+) (dirty):(\S+) (writeback):(\S+)")
, re.compile("(slab_reclaimable):(\S+) (slab_unreclaimable):(\S+)")
, re.compile("(mapped):(\S+) (shmem):(\S+) (pagetables):(\S+) bounce:\S+")
, re.compile("(free):(\S+) (free_pcp):(\S+) (free_cma):(\S+)")])
def oom_get_meminfo(oom_result, lines, index, num):
    oom = oom_result['sub_msg'][num]
    oom['meminfo']['slab'] = 0
    oom['meminfo']['slabr'] = 0
    oom['meminfo']['active_anon'] = 0
    oom['meminfo']['inactive_anon'] = 0
    oom['meminfo']['active_file'] = 0
    oom['meminfo']['inactive_file'] = 0
    oom['meminfo']['unevictable'] = 0
    oom['meminfo']['pagetables'] = 0
    oom['meminfo']['free'] = 0
    oom['meminfo']['free_pcp'] = 0
    oom['meminfo']['rmem'] = 0
    oom['meminfo']['hugepage'] = 0
    oom['meminfo']['total_mem'] = 0
    if len(lines) < 10:
        return True
    line = lines
    for key in range(index, len(lines)):
        line = lines[key]
        if line.find('active_anon:') != -1 and line.find('inactive_anon:') != -1:
            break
    if key >= len(lines) -5:
        return True

    index = key
    for pattern in meminfo_pattern:
        line = lines[index]
        index += 1
        gp = pattern.search(line)
        if gp:
            for i in range(1,len(gp.groups()),2):
                if gp.group(i) == 'slab_unreclaimable':
                    oom['meminfo']['slab'] = int(gp.group(i+1))*4
                if gp.group(i) == 'slab_reclaimable':
                    oom['meminfo']['slabr'] = int(gp.group(i+1))*4
                else:
                    oom['meminfo'][gp.group(i)] = int(gp.group(i+1))*4
    return True

def oom_get_total_mem(oom_result, line, num):
    if "pages RAM" not in line:
        return True
    total = line.strip().split(']')[1].strip().split()[0]
    total = int(total)*4
    oom_result['sub_msg'][num]['meminfo']['total_mem'] = total
    return True

def oom_get_rmem(oom_result, line, num):
    if "pages reserved" not in line:
        return True
    rmem = line.strip().split(']')[1].strip().split()[0]
    rmem = int(rmem)*4
    oom_result['sub_msg'][num]['meminfo']['rmem'] = rmem

def oom_is_cgroup_oom(cgroup):
    return cgroup == OOM_REASON_PCGROUP or cgroup == OOM_REASON_CGROUP


def oom_costly_order(order):
    return order >=1 and order <=3

def oom_is_memfrag_oom(oom):
    free = oom['host_free']
    low = oom['host_low']
    order = oom['order']
    memfrag = False
    if free > low and oom_costly_order(order):
        memfrag = True
    return memfrag


def memleak_check(total, kmem):
    kmem = kmem/1024
    total = total/1024
    thres = 1024*6

    ''' 100G '''
    if total > 100*1024:
        thres = 1024*10
    if kmem > thres:
        return True
    elif (kmem > total*0.1) and (kmem > 1024*1.5):
        return True
    return False

def oom_is_memleak(oom, oom_result):
    if 'meminfo' not in oom:
        return False
    if 'slab' not in oom['meminfo']:
        return False
    meminfo = oom['meminfo']
    res = oom['json']
    total = meminfo['total_mem']
    used = total - meminfo['slab'] - meminfo['slabr']
    used = used - (meminfo['active_anon'] + meminfo['inactive_anon'])
    used -= (meminfo['active_file'] + meminfo['inactive_file'])
    used -= (meminfo['unevictable'] + meminfo['pagetables'])
    used -= (meminfo['free'] + meminfo['hugepage'])
    if memleak_check(total, meminfo['slab']):
        res['leaktype'] = 'slab'
        res['leakusage'] = meminfo['slab']
        return "slab memleak, usage:%dkb\n"%(meminfo['slab'])
    elif memleak_check(total, used):
        res['leaktype'] = 'allocpage'
        res['leakusage'] = used
        return "allocpage memleak, usage:%dkb\n"%(used)
    return False

def oom_host_output(oom_result, num):
    oom = oom_result['sub_msg'][num]
    reason = oom['reason']
    res = oom['json']
    summary = ''
    if not oom_is_host_oom(reason):
        return summary
    free = int(oom['host_free'][:-2])
    low = int(oom['host_low'][:-2])
    is_low = False
    if free * 0.9  < low:
        is_low = True
    oom['root'] = 'limit'
    if 'mems_allowed' in oom and oom['mems_allowed'][0] != -1 and oom_result['node_num'] != len(oom['mems_allowed']) and is_low:
        oom['reason'] = OOM_REASON_NODE
        oom['root'] = 'cpuset'
        summary += "total node:%d\n"%(oom_result['node_num'])
        summary += "cpuset:%s,"%(oom['cpuset'])
        summary += "cpuset config:"
        for node in oom['mems_allowed']:
            summary +="%s "%(node)
        summary += "\n"
        summary += "node free:%s,"%(oom['host_free'])
        summary += "low:%s\n"%(oom['host_low'])
        return summary
    elif 'nodemask' in oom and oom['nodemask'][0] != -1 and len(oom['nodemask']) != oom_result['node_num'] and free > low * 2:
        oom['reason'] = OOM_REASON_NODEMASK
        oom['root'] = 'policy'
        summary += "total node:%d\n"%(oom_result['node_num'])
        summary += "nodemask config:"
        for node in oom['nodemask']:
            summary +="%s "%(node)
        summary += "\n"
        summary += "node free:%s,"%(oom['host_free'])
        summary += "low:%s\n"%(oom['host_low'])
        return summary
    elif oom_is_memfrag_oom(oom):
        summary += "order:%d\n"%(oom['order'])
        oom['reason'] = OOM_REASON_MEMFRAG
        oom['root'] = 'frag'
        #oom['json']['order'] = oom['order']
    leak = oom_is_memleak(oom, oom_result)
    if leak != False:
        oom['reason'] = OOM_REASON_MEMLEAK
        oom['root'] = 'memleak'
        summary += leak
    summary += "host free:%s,"%(oom['host_free'])
    summary += "low:%s\n"%(oom['host_low'])
    res['host_free'] = oom['host_free']
    res['host_low'] = oom['host_low']
    return summary

def oom_cgroup_output(oom_result, num):
    summary = ''
    oom = oom_result['sub_msg'][num]
    reason = oom['reason']
    res = oom['json']
    if not oom_is_cgroup_oom(reason):
        return summary
    pre = "cgroup"
    if oom['podName'] != 'unknow':
        pre = 'pod'
    elif oom['containerID'] != 'unknow':
        pre = 'container'
    summary += "%s memory usage: %s,"%(pre, oom['cg_usage'])
    summary += " limit: %s\n"%(oom['cg_limit'])
    summary += "oom cgroup: %s\n"%(oom['cg_name'])
    res['cg_usage'] = oom['cg_usage']
    res['cg_limit'] = oom['cg_limit']
    return summary

def oom_get_ipcs(oom_result, shmem):
    if oom_result['mode'] == 2:
        return False
    if not os.path.exists('/proc/sysvipc/shm'):
        return False
    fd = open('/proc/sysvipc/shm', 'r')
    lines = fd.readlines()
    fd.close()
    ipcs = 0
    for line in lines:
        if line.find('nattch') != -1 or line.find('shmid') != -1:
            continue
        line = line.split()
        ipcs += int(line[-2])
    if ipcs > shmem:
        return False
    if ipcs > shmem*0.55 * 1024:
        return True
    return False

def oom_cgroup_output_ext(oom_result, num):
    summary = ''
    oom = oom_result['sub_msg'][num]
    reason = oom['reason']
    res = oom['json']
    if not oom_is_cgroup_oom(reason):
        return summary
    if oom['root'] == 'limit' and reason ==  OOM_REASON_PCGROUP:
        oom['root'] = 'plimit'
    anon = int(oom["cg_inanon"]) + int(oom["cg_anon"]) - int(oom["cg_rss"])
    ipcs = False
    if anon > int(oom['cg_usage'][:-2])*0.3:
        ipcs = oom_get_ipcs(oom_result, anon)
        if ipcs == True:
            msg = "need to cleanup ipcs"
        else:
            msg = "need to cleanup tmpfs file"
        summary = ",but shmem usage %dKB,%s"%(anon, msg)
        oom['root'] = 'shmem'
        res['shmem'] = anon
    return summary


def oom_check_score(oom, oom_result):
    res = oom_result['max']
    res_total = oom_result['max_total']
    summary = ''
    if res['pid'] == 0:
        return False, "\n"
    if int(oom['pid'].strip()) == res['pid']:
        return False, '\n'
    if res['score'] >= 0:
        return False, "\n"
    many = False
    if (res_total['cnt']) > 2 and (res_total['rss']*0.8 > res['rss']):
        many = True
    if res['task'] == res_total['task'] or many == False:
        return True, '，process:%s(%s) memory usage: %dKB,oom_score_adj:%s\n'%(res['task'],res['pid'],res['rss']*4,res['score'])

    return False, "\n"

def oom_check_dup(oom, oom_result):
    res = oom_result['max']
    res_total = oom_result['max_total']
    summary = '\n'
    if (res_total['rss']*4 > oom['killed_task_mem']*1.5) and (res_total['cnt'] > 2):
        oom['root'] = 'fork'
        summary = ',%d process:%s total memory usage: %dKB\n'%(res_total['cnt'],res_total['task'],res_total['rss']*4)
        oom['json']['fork_max_task'] = res_total['task']
        oom['json']['fork_max_cnt'] = res_total['cnt']
        oom['json']['fork_max_usage'] = res_total['rss'] * 4
    return summary

def oom_get_podName(cgName, cID, oom_result):
    podName = 'unknow'
    if oom_result['mode'] == 2:
        return podName
    if cgName.find("kubepods") == -1:
        return podName
    cmd = "crictl inspect " + cID +" 2>/dev/null" +" | grep -w io.kubernetes.pod.name "
    res = os.popen(cmd).read().strip()
    if res.find("io.kubernetes.pod.name") == -1:
        return podName
    res = res.split()
    if len(res) < 2:
        return podName
    if res[0].find("io.kubernetes.pod.name") != -1:
        podName = res[1][1:-2]
    return podName

def oom_get_k8spod(oom_result,num):
    oom = oom_result['sub_msg'][num]
    res = oom['json']
    summary = ''
    cgName = oom['cg_name']
    oom['podName'] = 'unknow'
    oom['containerID'] = 'unknow'
    index = cgName.find("cri-containerd-")
    if index != -1:
        index = index + 15
    if index == -1:
        index = cgName.find("docker-")
        if index != -1:
            index = index + 7
    if index != -1:
        oom['containerID'] = cgName[index: index+13]
        oom['podName'] = oom_get_podName(cgName, oom['containerID'], oom_result)
    summary += "podName: %s, containerID: %s\n"%(oom['podName'], oom['containerID'])
    res['podName'] = oom['podName']
    res['containerID'] = oom['containerID']
    return summary

def oom_init_json(oom_result, num):
    oom = oom_result['sub_msg'][num]
    oom['json'] = {}
    res = oom['json']
    res['task'] = 'unknow'
    res['pid'] = 'unknow'
    res['task_mem'] = 0
    res['total_rss'] = 0
    res['root'] = 'unknow'
    res['type'] = 'unknow'
    res['podName'] = 'unknow'
    res['containerID'] = 'unkonw'
    res['cg_usage'] = 0
    res['cg_limit'] = 0
    res['leaktype'] = 'unknow'
    res['leakusage'] = 0
    res['shmem'] = 0

def oom_output_msg(oom_result,num, summary):
    oom = oom_result['sub_msg'][num]

    oom_init_json(oom_result, num)
    oom['json']['rss_list_desc'] = oom['rss_list_desc']
    res = oom['json']
    res['total_oom'] = oom_result['oom_total_num']
    res['cg_name'] = oom['cg_name']
    res['host_free'] = oom.get('host_free',0)
    res['host_low'] = oom.get('host_low',0)
    reason = ''
    #print("oom time = {} spectime = {}".format(oom['time'], oom_result['spectime']))
    task = oom['task_name']
    task_mem = oom['killed_task_mem']
    if task_mem == 0 and oom['pid'] in oom['state_mem']:
        task_mem = oom['state_mem'][oom['pid']]
        oom['killed_task_mem'] = task_mem
    res['task'] = task[1:-1]
    res['pid'] = oom['pid']
    res['task_mem'] = task_mem
    res['total_rss'] = oom['state_mem']['total_rss']
    summary += "total rss: %d KB\n"%(oom['state_mem']['total_rss'])
    summary += "task: %s(%s), memory usage: %sKB\n"%(task[1:-1], oom['pid'], task_mem)
    #summary += "进程Kill次数:%s,进程内存占用量:%sKB\n"%(oom_result['task'][task], oom['killed_task_mem']/1024)
    #summary += "oom cgroup:%s"%(oom['cg_name'])
    oom['root'] = 'limit'
    if oom['cg_name'] in oom_result['cgroup']:
        #summary += "oom总次数:%s\n"%(oom_result['cgroup'][oom['cg_name']])
        summary += oom_get_k8spod(oom_result, num)
    summary += oom_cgroup_output(oom_result, num)
    #summary += "oom cgroup: %s\n"%(oom['cg_name'])
    summary += oom_host_output(oom_result, num)
    reason = "diagnones result: %s "%(oom['reason'])
    reason += oom_cgroup_output_ext(oom_result, num)
    ret, sss = oom_check_score(oom, oom_result)
    if ret == False:
        reason+= oom_check_dup(oom, oom_result)
    else:
        reason += sss
    if oom['type']  == 'cgroup':
        if 'msg' in oom['state_mem']:
            summary += "memory stats:\n"
            for line in oom['state_mem']['msg']:
                summary += line +'\n'
    summary += "type: %s, root: %s\n"%(oom['type'], oom['root'])
    res['root'] = oom['root']
    res['type'] = oom['type']
    res['result'] = reason
    res['msg'] = summary
    return reason + summary

def oom_get_max_task(num, oom_result):
    oom = oom_result['sub_msg'][num]
    dump_task = False
    res = oom_result['max']
    res_total = oom_result['max_total']
    rss_all = {}
    state = oom['state_mem']
    state['msg'] = []
    state['total_rss'] = 0
    for line in oom['oom_msg']:
        try:
            if 'rss' in line and 'oom_score_adj' in line and 'name' in line:
                dump_task = True
                state['msg'].append(line)
                continue
            if not dump_task:
                continue
            if 'Out of memory' in line:
                break
            if OOM_END_KEYWORD in line or OOM_END_KEYWORD_4_19 in line:
                break
            if line.count('[') !=2 or line.count(']') !=2:
                break
            pid_idx = line.rfind('[')
            last_idx = line.rfind(']')
            if pid_idx == -1 or last_idx == -1:
                continue
            pid = int(line[pid_idx+1:last_idx].strip())
            last_str = line[last_idx+1:].strip()
            last = last_str.split()
            if len(last) < 3:
                continue
            if last[-1] not in rss_all:
                rss_all[last[-1]] = {}
                rss_all[last[-1]]['rss'] = int(last[3])
                rss_all[last[-1]]['cnt'] = 1
            else:
                rss_all[last[-1]]['rss'] += int(last[3])
                rss_all[last[-1]]['cnt'] += 1
            state['msg'].append(line)
            state[str(pid)] = int(last[3]) *4
            state['total_rss'] += int(last[3]) *4
            if int(last[3]) >  res['rss']:
                res['rss'] = int(last[3])
                res['score'] = int(last[-2])
                res['task'] = last[-1]
                res['pid'] = pid
            if rss_all[last[-1]]['rss'] >  res_total['rss']:
                res_total['rss'] = int(rss_all[last[-1]]['rss'])
                res_total['cnt'] = int(rss_all[last[-1]]['cnt'])
                res_total['score'] = int(last[-2])
                res_total['task'] = last[-1]
        except Exception as err:
            sys.stderr.write("oom_get_max_task loop err {} lines {}\n".format(err, traceback.print_exc()))
            continue
    oom['rss_all'] = rss_all
    tmprss = sorted(oom['rss_all'].items(),key=lambda k:k[1]['rss'], reverse=True)[0:10]
    oom['rss_list_desc'] = []
    for task_info in tmprss:
        task = task_info[0]
        oom['rss_list_desc'].append({'task':task, 'rss':oom['rss_all'][task]['rss']})
    return res

def oom_reason_analyze(num, oom_result, summary):
    try:
        node_num = 0
        lines = oom_result['sub_msg'][num]['oom_msg']
        line_len = len(lines)
        for key in range(line_len):
            try:
                line = lines[key]
                if "invoked oom-killer" in line:
                    oom_get_order(oom_result, line, num)
                if 'nodemask' in line:
                    oom_get_nodemask(oom_result, line, num)
                if "mems_allowed=" in line:
                    oom_get_mem_allowed(oom_result, line, num)
                elif "Task in" in line:
                    oom_get_cgroup_name(oom_result, line, num)
                elif "memory: usage" in line:
                    oom_get_cgroup_mem(oom_result, line, num)
                elif "Memory cgroup stats for" in line:
                    oom_get_cgroup_shmem(oom_result, line, num)
                elif "Normal free:" in line:
                    oom_get_host_mem(oom_result, line, num)
                elif "Mem-Info:" in line:
                    oom_get_meminfo(oom_result, lines, key,num)
                elif "pages RAM" in line:
                    oom_get_total_mem(oom_result, line, num)
                elif "pages reserved" in line:
                    oom_get_rmem(oom_result, line, num)
                elif line.find('hugepages_total')!=-1:
                    if oom_is_node_num(line):
                        node_num += 1
                    oom_get_hugepage(oom_result, line, num)
                elif OOM_END_KEYWORD in line or OOM_END_KEYWORD_4_19 in line:
                    oom_get_task_mem(oom_result, line, num)
                    oom_get_pid(oom_result, line, num)
            except Exception as err: 
                sys.stderr.write("oom_reason_analyze loop err {} lines {}\n".format(err, traceback.print_exc()))
                continue
        oom_result['node_num'] = node_num
        summary = oom_output_msg(oom_result, num, summary)
        oom_result['sub_msg'][num]['summary'] = summary
        if oom_result['json'] == 1:
            #print(json.dumps(oom_result['sub_msg'][num]['json'], encoding='utf-8', ensure_ascii=False))
            #print(json.dumps(oom_result['sub_msg'][num]['json'], ensure_ascii=False))
            pass
        else:
            print(summary)
        return summary
    except Exception as err:
        sys.stderr.write("oom_reason_analyze err {} lines {}\n".format(err, traceback.print_exc()))
        return ""

def oom_dmesg_analyze(dmesgs, oom_result):
    try:
        OOM_END_KEYWORD_real = OOM_END_KEYWORD
        if OOM_BEGIN_KEYWORD not in dmesgs:
            return
        dmesg = dmesgs.splitlines()
        oom_getting = 0
        task_name = "-unknow-"
        if task_name not in oom_result['task']:
            oom_result['task'][task_name] = 0
        for line in dmesg:
            line = line.strip()
            if len(line) > 0 and OOM_BEGIN_KEYWORD in line:
                oom_result['oom_total_num'] += 1
                oom_getting = 1
                oom_result['sub_msg'][oom_result['oom_total_num']] = {}
                oom_result['sub_msg'][oom_result['oom_total_num']]['oom_msg'] = []
                oom_result['sub_msg'][oom_result['oom_total_num']]['time'] = 0
                oom_result['sub_msg'][oom_result['oom_total_num']]['cg_name'] = 'unknow'
                oom_result['sub_msg'][oom_result['oom_total_num']]['task_name'] = task_name
                oom_result['sub_msg'][oom_result['oom_total_num']]['pid'] = "0"
                oom_result['sub_msg'][oom_result['oom_total_num']]['killed_task_mem'] = 0
                oom_result['sub_msg'][oom_result['oom_total_num']]['state_mem'] = {}
                oom_result['sub_msg'][oom_result['oom_total_num']]['meminfo'] = {}
                oom_result['sub_msg'][oom_result['oom_total_num']]['type'] = 'unknow'
                oom_result['sub_msg'][oom_result['oom_total_num']]['root'] = 'unknow'
                oom_result['sub_msg'][oom_result['oom_total_num']]['reason'] = ''
                oom_result['sub_msg'][oom_result['oom_total_num']]['summary'] = ''
                if line.find('[') != -1:
                    oom_result['sub_msg'][oom_result['oom_total_num']]['time'] = oom_time_to_normal_time(line.split('[')[1].split(']')[0])
                oom_result['time'].append(oom_result['sub_msg'][oom_result['oom_total_num']]['time'])
            if oom_getting == 1:
                oom_result['sub_msg'][oom_result['oom_total_num']]['oom_msg'].append(line)
                if OOM_END_KEYWORD in line or OOM_END_KEYWORD_4_19 in line:
                    if OOM_END_KEYWORD_4_19 in line:
                        OOM_END_KEYWORD_real = OOM_END_KEYWORD_4_19
                    if OOM_END_KEYWORD in line:
                        OOM_END_KEYWORD_real = OOM_END_KEYWORD
                    oom_getting = 0
                    task_name = line.split(OOM_END_KEYWORD_real)[1].split()[1].strip(',')
                    oom_result['sub_msg'][oom_result['oom_total_num']]['task_name'] = task_name
                    if task_name not in oom_result['task']:
                        oom_result['task'][task_name] = 1
                    else:
                        oom_result['task'][task_name] += 1

                if OOM_CGROUP_KEYWORD in line:
                    cgroup_name = line.split('Task in')[1].split()[0]
                    oom_result['sub_msg'][oom_result['oom_total_num']]['cgroup_name'] = cgroup_name
                    #print cgroup_name
                    if cgroup_name not in oom_result['cgroup']:
                        oom_result['cgroup'][cgroup_name] = 1
                    else:
                        oom_result['cgroup'][cgroup_name] += 1

    except Exception as err:
        sys.stderr.write("oom_dmesg_analyze failed {}\n".format(err))

def oom_read_dmesg(data, mode, filename):
    if mode == 1:
        cmd = 'dmesg -T 2>/dev/null'
        output = os.popen(cmd)
        dmesgs = output.read().strip()
        output.close()
        data['dmesg'] = dmesgs
    elif mode == 2:
       with open(filename, 'r') as f:
           data['dmesg'] = f.read().strip()

def oom_diagnose(sn, data, mode):
    try:
        oom_result = {}
        oom_result['task'] = ""
        oom_result['json'] = data['json']
        oom_result['mode'] = mode
        oom_result['summary'] = ""
        oom_result['oom_total_num'] = 0
        oom_result['cgroup'] = {}
        oom_result['task'] = {}
        oom_result['sub_msg'] = {}
        oom_result['last_time'] = {}
        oom_result['time'] = []
        oom_result['spectime'] = data['spectime']
        oom_result['max'] = {'rss':0,'task':"",'score':0,'pid':0}
        oom_result['max_total'] = {'rss':0,'task':"",'score':0,'cnt':0}
        dmesgs = data['dmesg']
        if OOM_BEGIN_KEYWORD in dmesgs:
            oom_dmesg_analyze(dmesgs, oom_result)
            oom_result['summary'] += "total oom: %s\n"%oom_result['oom_total_num']

            sorted_tasks = sorted(oom_result['task'].items(), key = lambda kv:(kv[1], kv[0]), reverse=True)
            sorted_cgroups = sorted(oom_result['cgroup'].items(), key = lambda kv:(kv[1], kv[0]), reverse=True)
            last_oom = oom_result["oom_total_num"]
            num = oomcheck_get_spectime(oom_result['spectime'], oom_result)
            if num < 0 or num > last_oom:
                num = last_oom
            last_num = num-data['num']+1
            if last_num <= 0 :
                last_num = 1
            output_json = {}
            for i in range(last_num,num+1):
                oom_get_max_task(i, oom_result)
                submsg = oom_reason_analyze(i, oom_result, oom_result['summary'])
                output_json[str(oom_result['sub_msg'][i]['time'])] = oom_result['sub_msg'][i]['json']
            if oom_result['json'] == 1:
                print(json.dumps(output_json, ensure_ascii=False))
            #res = oom_get_max_task(num, oom_result)
            #submsg = oom_reason_analyze(num, oom_result, oom_result['summary'])
            oom_result['summary'] = submsg
        data['oom_result'] = oom_result
        return oom_result['summary']

    except Exception as err:
        import traceback
        traceback.print_exc()
        print( "oom_diagnose failed {}".format(err))
        data['oom_result'] = oom_result
        return oom_result['summary']

#
# mode = 1 for  live mode
# mode = 2 for file mode
def main():
    sn = ''
    data = {}
    data['mode'] = 1
    data['json'] = 0
    data['num'] = 1
    data['filename'] = ''
    data['spectime'] = int(time.time())
    get_opts(data)
    oom_read_dmesg(data, data['mode'], data['filename'])
    oom_diagnose(sn, data, data['mode'])


def usage():
    print(
        """
            -h --help     print the help
            -f --dmesg file
            -l --live mode
            -t --time mode
            -j --output json
            -n --# of output results
           for example:
           sysak oomcheck.py
           sysak oomcheck.py -t "2021-09-13 15:32:22"
           sysak oomcheck.py -t 970665.476522
           sysak oomcheck.py -f oom_file.txt
           sysak oomcheck.py -f oom_file.txt -t 970665.476522
        """
    )

def get_opts(data):
    options,args = getopt.getopt(sys.argv[1:],"jhlf:t:n:",["json","help","file=","live=","time="])
    for name,value in options:
        if name in ("-h","--help"):
            usage()
            sys.exit(0)
        elif name in ("-f","--file"):
            data['mode'] = 2
            data['filename'] = value
        elif name in ("-l","--live"):
            data['mode'] = 1
        elif name in ("-j","--json"):
            data['json'] = 1
        elif name in ("-n","--num"):
            data['num'] = int(value)
        elif name in ("-t","--time"):
            if '-' in value:
                value = normal_time2ts(value)
            data['spectime'] = float(value)

if __name__ == "__main__":
    main()
