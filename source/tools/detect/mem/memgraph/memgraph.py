#!/usr/bin/env python3
import os
import sys
import json
import  getopt
from ctypes import c_int
from ctypes import c_size_t
from ctypes import c_ulong
from ctypes import c_ubyte
from ctypes import c_void_p
from ctypes import c_longlong
from ctypes import get_errno
from ctypes import CDLL
from ctypes import POINTER
from ctypes import cast
from mmap import MAP_SHARED
from mmap import PROT_READ
from mmap import PAGESIZE

jsonFormat = None

if os.geteuid() != 0:
    print("This program must be run as root. Aborting.")
    sys.exit(1)

try:
    from ctypes import c_ssize_t
    c_off_t = c_ssize_t
except ImportError:
    is_64bits = sys.maxsize > 2 ** 32
    c_off_t = c_longlong if is_64bits else c_int

MAP_FAILED = c_ulong(-1).value
libc = CDLL(None)

_mmap = libc.mmap
_mmap.restype = c_void_p
_mmap.argtypes = c_void_p, c_size_t, c_int, c_int, c_off_t

_munmap = libc.munmap
_munmap.restype = c_void_p
_munmap.argtypes = c_void_p, c_size_t

_mincore = libc.mincore
_mincore.restypes = c_int
_mincore.argtypes = c_void_p, c_size_t, POINTER(c_ubyte)

def getcache(filename):
    if os.path.isfile(filename) and os.access(filename, os.R_OK):
        f = open(filename)
        size = os.fstat(f.fileno()).st_size
        if size == 0:
            f.close()
            return 0
        addr = _mmap(0, size, PROT_READ, MAP_SHARED, f.fileno(), 0)
        if addr == MAP_FAILED:
            f.close()
            return 0

        nr_pages = int((size + PAGESIZE - 1) / PAGESIZE)
        vec = (c_ubyte * nr_pages)()
        ret = _mincore(addr, size, cast(vec, POINTER(c_ubyte)))
        if ret != 0:
            _munmap(addr, size)
            f.close()
            return 0
        cached = list(vec).count(1)
        _munmap(addr, size)
        f.close()
        return cached
    else:
        return 0

def getCachePid(meminfo,filename):
    res = []
    filepid = meminfo["filepid"]
    taskInfo = meminfo["taskInfo"]
    if not filename in filepid:
        return res
    for pid in filepid[filename]:
        if not pid in taskInfo:
            continue
        name = taskInfo[pid]["Name"]
        res.append(name + "_" + pid +" ")
    return res

def getFileCache(meminfo):
    total_cache = 0
    filecache = {}
    for tmpfile in meminfo["filepid"].keys():
        cache = getcache(tmpfile)
        total_cache += cache
        if cache > 0:
            filecache[tmpfile] = cache
    meminfo["filecache"] = sorted(filecache.items(), key = lambda kv:(kv[1], kv[0]),reverse=True)
    meminfo["fileCacheMem"] = total_cache*4
    #print("total CacheMem {}".format(meminfo["fileCacheMem"]))
    #print(meminfo["filecache"])
    global jsonFormat
    if jsonFormat != None:
        return meminfo
    num = 0
    for key,value in meminfo["filecache"]:
        filename = key
        filename += " cached:%sK"%(value*4)
        filename += " task: "
        filename += "".join(getCachePid(meminfo, key))
        num += 1
        print(filename)
        if num > 10:
            break
    return meminfo

def is_number(s):
    try:
        float(s)
        return True
    except ValueError:
        pass
    try:
        import unicodedata
        unicodedata.numeric(s)
        return True
    except (TypeError, ValueError):
        pass
    return False

def hugepagesz_supported(hugepagesz):
    if int(hugepagesz) != 2 and int(hugepagesz) != 1024:
        return False
    if os.path.exists("/sys/kernel/mm/hugepages/hugepages-" + str(hugepagesz * 1024) + "kB"):
            return True
    return False


def get_dmesg_reserved_mem(meminfo):
    cmd = "dmesg | grep -w 'Memory:' | grep -w 'reserved' "
    ret = os.popen(cmd).read().strip()
    if len(ret) < 20:
        return 0
    ret = ret.strip().split("reserved")[0]
    ret = ret.strip().split()
    if len(ret) < 5:
        return 0
    res = ret[-1].strip()[:-1]
    return int(res)

def get_total_mem(meminfo):
    cmd = "cat /proc/iomem | grep 'System RAM' "
    ret = os.popen(cmd).read().split("\n")
    total = 0
    for line in ret:
        line = line.strip()
        if len(line) < 10:
            continue
        addr = line.split(":")[0].strip()
        start = addr.split("-")[0].strip()
        end = addr.split("-")[1].strip()
        size = int(end,16) - int(start,16)
        total += size
    meminfo['MemTotalIoMem'] = total/1024

def get_reserved_mem(meminfo):
    res = get_dmesg_reserved_mem(meminfo)
    if res != 0:
        meminfo['res'] = res
        return res

    cmd = "cat /proc/iomem | grep -wE 'Reserved|Crash kernel|Kernel code|Kernel data|Kernel bss'"
    ret = os.popen(cmd).read().split("\n")
    total = 0 
    for line in ret:
        line = line.strip()
        if len(line) < 10: 
            continue
        addr = line.split(":")[0].strip()
        start = addr.split("-")[0].strip()
        end = addr.split("-")[1].strip()
        size = int(end,16) - int(start,16)
        total += size
    meminfo['res'] = total/1024

def get_page_used(meminfo):
    get_reserved_mem(meminfo)
    #get_total_mem(meminfo)
    user = meminfo["Active(anon)"] + meminfo["Inactive(anon)"]
    user += meminfo["Active(file)"] + meminfo["Inactive(file)"]
    user += meminfo["Mlocked"]
    if "2048" in meminfo:
        user += meminfo["2048"]
    if "1048576" in meminfo:
        user += meminfo["1048576"]
    kernelOther = meminfo["Slab"] + meminfo["KernelStack"] + meminfo["PageTables"]
    kernelOther += meminfo["VmallocUsed"]
    pageUsed = meminfo["MemTotal"] - meminfo["MemFree"] - user - kernelOther
    if pageUsed < 1:
        pageUsed = 1024
    meminfo["allocPage"] = pageUsed
    meminfo["kernelUsed"] = pageUsed + kernelOther + meminfo["res"]
    meminfo["userUsed"] = user
    return meminfo

def get_hugepage_used(meminfo, hugesize):
    hugepath = "/sys/kernel/mm/hugepages/hugepages-"
    hugepath += hugesize+"kB/nr_hugepages"
    fd = open(hugepath, 'r')
    res = int(fd.read().strip())
    res = res * int(hugesize)
    meminfo[hugesize] = res
    return meminfo

def get_VmallocUsed(meminfo):
    if meminfo["VmallocUsed"] != 0:
        return meminfo
    fd = open("/proc/vmallocinfo", 'r')
    ret = fd.read().split('\n')
    fd.close()
    pages = 0
    for line in ret:
        if line.find("vmalloc") == -1:
            continue
        if line.find("pages=") == -1:
            continue
        res = line.strip().split("pages=")
        res = int(res[1].strip().split()[0])
        pages += res
    meminfo["VmallocUsed"] = pages*4

def memgraph_get_meminfo(meminfo):
    fd = open("/proc/meminfo",'r')
    ret = fd.read().split('\n')
    meminfo["rawdata"] = ret
    for i in ret:
        line = i.strip().split()
        if len(line) < 3:
            continue
        name = line[0].strip()[:-1]
        size = int(line[1].strip())
        meminfo[name] = size
    if hugepagesz_supported(2):
        get_hugepage_used(meminfo, "2048")
    if hugepagesz_supported(1024):
        get_hugepage_used(meminfo, "1048576")
    get_VmallocUsed(meminfo)
    get_page_used(meminfo)
    fd.close()
    return meminfo

def dump2json(res,filename):
    jsonStr = json.dumps(res)
    if not os.path.exists(os.path.dirname(filename)):
        os.popen("mkdir -p "+os.path.dirname(filename)).read()
    with open(filename, 'w') as jsonFile:
        jsonFile.write(jsonStr)

def memgraph_free(meminfo):
    cmd = "free -k"
    used = 0
    ret = os.popen(cmd).read().strip().split("\n")
    for line in ret:
        if line.find("Mem:") == -1:
            continue
        used = int(line.strip().split()[2])
    return used

def format_unit(value):
    value = int(value)
    if value > 5 * 1024 * 1024:
        return '%s KB (%s GB)' % (value, round(value/(1024*1024),2))
    elif value > 10 * 1024:
        return '%s KB (%s MB)' % (value, round(value/(1024),2))
    else:
        return '%s KB' % value

def memgraph_graph(meminfo):
    res = {}
    res["total"] = meminfo["MemTotal"] + meminfo['res']
    res["free"] = meminfo["MemFree"]
    res["userUsed"] = meminfo["userUsed"]
    res["kernelUsed"] = meminfo["kernelUsed"]
    res["available"] = meminfo["MemAvailable"]
    res["used"] = memgraph_free(meminfo) + meminfo['Shmem']
    user = {}
    user["anon"] = meminfo["Active(anon)"] + meminfo["Inactive(anon)"]
    user["filecache"] = meminfo["Cached"] - meminfo["Shmem"]
    user["buffers"] = meminfo["Buffers"]
    user["mlock"] = meminfo["Mlocked"]
    if "2048" in meminfo:
        user["huge2M"] = meminfo["2048"]
    if "1048576" in meminfo:
        user["huge1G"] = meminfo["1048576"]
    user["shmem"] = meminfo["Shmem"]
    res["user"] = user
    kernel = {}
    kernel["reserved"] = meminfo["res"]
    kernel["SReclaimable"] = meminfo["SReclaimable"]
    kernel["SUnreclaim"] = meminfo["SUnreclaim"]
    kernel["KernelStack"] = meminfo["KernelStack"]
    kernel["PageTables"] = meminfo["PageTables"]
    kernel["VmallocUsed"] = meminfo["VmallocUsed"]
    kernel["allocPage"] = meminfo["allocPage"]
    res["kernel"] = kernel

    format_res = {}
    for i in res:
        if type(res[i]) == dict:
            format_res[i] = {}
            for j in res[i]:
                format_res[i][j] = format_unit(res[i][j])
        else:
            format_res[i] = format_unit(res[i])

    meminfo["graph"] = res
    global jsonFormat
    if jsonFormat != None:
        return meminfo
    memgraph_check_event(meminfo)
    res_str = ("\nmemgraph result:\ntotal memory: %s\tused: %s\tfree: %s\tavailable: %s\n\nuser: total used: %s\n\tuser-anon: %s\n\tuser-filecache: %s\n\tuser-buffers: %s\n\tuser-mlock: %s\n\nkernel: total used: %s\n\tkernel-reserved: %s\n\tkernel-SReclaimable: %s\n\tkernel-SUnreclaim: %s\n\tkernel-PageTables: %s\n\tkernel-VmallocUsed: %s\n\tkernel-KernelStack: %s\n\tkernel-allocPage: %s\n") % (format_res['total'],format_res['used'],format_res['free'],format_res['available'],format_res['userUsed'],format_res['user']['anon'],format_res['user']['filecache'],format_res['user']['buffers'],format_res['user']['mlock'],format_res['kernelUsed'],format_res['kernel']['reserved'],format_res['kernel']['SReclaimable'],format_res['kernel']['SUnreclaim'],format_res['kernel']['PageTables'],format_res['kernel']['VmallocUsed'],format_res['kernel']['KernelStack'],format_res['kernel']['allocPage'])
    for item in meminfo['event']:
        if item == "util":
            continue
        if meminfo['event'][item] == True:
            if item == 'memcg':
                res_str = '%sNotice: memcg leak\n' % res_str
            elif item == 'leak':
                res_str = '%sNotice: memory leak\n' % res_str
            elif item == 'memfrag':
                res_str = '%sNotice: memory fragment\n' % res_str
    print(res)
    print(res_str)
    return res

def kmemleak_check(meminfo, memType):
    kmem = meminfo[memType]/1024
    total = meminfo["MemTotal"]/1024
    ''' 6G '''
    if kmem > 1024*6:
        return True
    elif (kmem*100 > total*10) and (kmem > 1024*1.5):
        return True
    return False

def get_proc_file(meminfo,pid):
    filename = "/proc/" + pid + "/fd"
    filepid = meminfo["filepid"]
    try:
        for fd in os.listdir(filename):
            fdpath = filename + "/" + fd
            if not os.path.exists(fdpath):
                continue
            tmp = os.readlink(fdpath).strip()
            if len(tmp) < 2:
                continue
            if tmp[0] != '/':
                continue
            if tmp[0:5] == "/dev/":
                continue
            if tmp[0:6] == "/proc/":
                continue
            if tmp[0:5] == "/sys/":
                continue
            if not tmp in filepid:
                filepid[tmp] = []
            filepid[tmp].append(pid)
        meminfo["filepid"] = filepid
    except Exception:
        pass
    return meminfo

def scan_proc(meminfo):
    root = "/proc/"
    taskInfo = {}
    taskMem = {}
    taskAnon = {}
    try:
        for pid in os.listdir(root):
            if not os.path.exists(root + pid):
                continue
            if not is_number(pid):
                continue
            filename= root + pid + "/status"
            get_proc_file(meminfo, pid)
            try:
                fd = open(filename, 'r')
                res = fd.read().strip()
                if res.find("RssAnon") == -1:
                    continue
                res = res.split('\n')
                fd.close()
                task = {}
                for line in res:
                    values = line.strip().split()
                    if len(values) < 2:
                        continue
                    key = values[0][:-1]
                    # get memory usage
                    value = values[1]
                    task[key] = value
                tmp = {}
                tmp["Name"] = task["Name"]
                tmp["Pid"] = task["Pid"]
                tmp["RssAnon"] = int(task["RssAnon"])
                tmp["RssFile"] = int(task["RssFile"])
                tmp["RssShmem"] = int(task["RssShmem"])
                taskInfo[pid] = tmp
                taskMem[pid] = int(task["VmRSS"])
                taskAnon[pid] = int(task["RssAnon"])
            except Exception:
                import traceback
                traceback.print_exc()
                pass
    except Exception :
        import traceback
        traceback.print_exc()
        pass
    #print(taskInfo)
    meminfo["taskInfo"] = taskInfo
    taskMem = sorted(taskMem.items(), key = lambda kv:(kv[1], kv[0]),reverse=True)
    taskAnon = sorted(taskAnon.items(), key = lambda kv:(kv[1], kv[0]),reverse=True)
    meminfo["taskMem"] = taskMem
    meminfo["taskAnon"] = taskAnon
    num = 0
    return meminfo

def read_cgroup_stat(filename):
    stat = {}
    filename += "/memory.stat"
    if not os.path.exists(filename):
        return stat
    fd = open(filename)
    ret = fd.read().strip().split("\n")
    for line in ret:
        line = line.strip().split()
        if len(line) < 2:
            continue
        stat[line[0]] = int(line[1])/1024
    return stat

def read_cgroup_proc(filename):
    task = {}
    filename += "/cgroup.procs"
    if not os.path.exists(filename):
        return task

    fd = open(filename)
    ret = fd.read().strip().split('\n')
    fd.close()
    if len(ret) == 0:
        return task
    for pid in ret:
        if not len(pid):
            continue
        pidpath = "/proc/" + pid +"/comm"
        try:
            fd = open(pidpath)
            ret = fd.read().strip()
            task[pid] = ret
            fd.close()
        except Exception:
            pass
    return task

def read_cgroup_usage(filename):
    filename += "/memory.usage_in_bytes"
    fd = open(filename)
    ret = int(fd.read().strip())
    fd.close()
    return ret/1024

def scan_cgroup(meminfo, filepath, find_depth):
    find_depth -= 1
    ignore_path=['.', '..']
    filename = ''
    depth = meminfo["cgroupOrigDepth"] - find_depth -1
    if not meminfo["cgroupTop"][depth]:
        meminfo["cgroupTop"][depth] = {}
    for file_ in os.listdir(filepath):
        filename = os.path.join(filepath, file_)
        if os.path.isdir(filename):
            usage = read_cgroup_usage(filename)
            #print("cgroup {} = {}K depth ={} task = {}".format(filename,usage, depth + 1, read_cgroup_proc(filename)))
            meminfo["cgroupTop"][depth][filename] = usage
            if depth + 1 > meminfo["cgroupDepth"]:
                meminfo["cgroupDepth"] = depth + 1
        if os.path.isfile(filename):
            continue
        elif find_depth <= 0:
            continue
        elif file_ in ignore_path:
            continue
        else:
            scan_cgroup(meminfo, filename, find_depth)

def memgraph_memory_thread(meminfo, name):
    global jsonFormat
    if len(meminfo[name]) == 0:
        scan_proc(meminfo)
    taskMem = meminfo[name]
    taskInfo = meminfo["taskInfo"]
    if jsonFormat != None:
        return meminfo
    num = 0
    for key,value in taskMem:
        task = taskInfo[key]
        num += 1
        if num > 10:
            break
        print("{}-{} Mem : {}K(RssFile : {}K RssAnon : {}K Shmem : {}K)".format(task["Name"], key, value, task["RssFile"], task["RssAnon"], task["RssShmem"]))

def memgraph_cache_list(meminfo):
    if len(meminfo["filepid"]) == 0:
        scan_proc(meminfo)
    getFileCache(meminfo)

def memgraph_kmemleak_check(meminfo):
    global jsonFormat
    res = {}
    res["leak"] = "No"
    res["type"] = "No"
    res["usage"] = 0
    if kmemleak_check(meminfo, "SUnreclaim") == True:
        res["leak"] = "Yes"
        res["type"] = "SUnreclaim"
        res["usage"] = meminfo["SUnreclaim"]
    elif kmemleak_check(meminfo, "allocPage") == True:
        res["leak"] = "Yes"
        res["type"] = "AllocPage"
        res["usage"] = meminfo["allocPage"]
    elif kmemleak_check(meminfo, "VmallocUsed") == True:
        res["leak"] = "Yes"
        res["type"] = "Vmalloc"
        res["usage"] = meminfo["VmallocUsed"]
    meminfo["memleak"] = res
    if jsonFormat != None:
        return meminfo
    print(res)

def memgraph_memory_cgroup(meminfo, depth):
    meminfo["cgroupOrigDepth"] = depth
    scan_cgroup(meminfo, "/sys/fs/cgroup/memory", depth)
    depth = meminfo["cgroupDepth"] -1
    cg={}
    i = 0
    for i in range(0, depth):
        if  len(meminfo["cgroupTop"][i]) == 0:
            continue
        for key, value in meminfo["cgroupTop"][i].items():
            find = 0
            for key2, value2 in meminfo["cgroupTop"][i+1].items():
                if key2.find(key) != -1 and value2 > value*0.6:
                    find = 1
                    break
            if find == 0:
                cg[key] = value
    for key,value in meminfo["cgroupTop"][depth].items():
        cg[key] = value
    meminfo["cgroupTop"] = sorted(cg.items(), key = lambda kv:(kv[1], kv[0]),reverse=True)
    num = 0
    cg = {}
    for key, value in meminfo["cgroupTop"]:
        tmp = {}
        tmp2 = {}
        tmp["total_size"] = value
        tmp["cache"] = 0
        tmp["rss"] = 0
        tmp2 = read_cgroup_stat(key)
        if len(tmp2) != 0:
            tmp["cache"] = tmp2["total_cache"]
            tmp["rss"] = tmp2["total_rss"]
        cg[key] = tmp
        num += 1
        if num >= 5:
            break
    meminfo["cgroupInfo"] = cg
    global jsonFormat
    if jsonFormat != None:
        return
    for key,value in meminfo["cgroupTop"]:
        if key in cg:
            print("{} : {}".format(key,cg[key]))

def memgrapth_output_json(meminfo, filepath):
    global jsonFormat
    if jsonFormat == None:
        return meminfo
    res = {}
    res["memGraph"] = meminfo["graph"]
    res["event"] = meminfo["event"]
    res["memleak"] = meminfo["memleak"]
    summary= ''
    if meminfo["event"]["leak"] == True:
       summary += " %s memory leak usage:%s"%(meminfo["memleak"]["type"], meminfo["memleak"]["usage"])
    if meminfo["event"]["memcg"] == True:
        summary += "  memory cgroup leak"
    if len(summary) == 0:
        summary = "success"
    res["summary"] = summary
    taskMem = meminfo["taskMem"]
    taskInfo = meminfo["taskInfo"]
    tmp_mem = []

    if len(taskMem) != 0:
        for key,value in taskMem:
            tmp_task = {}
            tmp_task["comm"] = taskInfo[key]["Name"]
            tmp_task["pid"] = key
            tmp_task["total_mem"] = value
            tmp_task["RssFile"] = taskInfo[key]["RssFile"]
            tmp_task["RssAnon"] = taskInfo[key]["RssAnon"]
            tmp_task["RssShmem"] = taskInfo[key]["RssShmem"]
            tmp_mem.append(tmp_task)
            if len(tmp_mem) >= 10:
                break
    res["memTop"] = tmp_mem
    cg_list = []
    if len(meminfo["cgroupTop"]) != 0:
        for key, value in meminfo["cgroupTop"]:
            tmp_cg = {}
            if key in meminfo["cgroupInfo"]:
                tmp_cg["cgroup"] = key
                tmp_cg.update(meminfo["cgroupInfo"][key])
                cg_list.append(tmp_cg)
    res["cgroupTop"] = cg_list

    cache_list = []
    if len(meminfo["filecache"]) != 0:
        for key,value in meminfo["filecache"]:
            tmp_cache = {}
            tmp_cache["file"] = key
            tmp_cache["cached"] = value *4
            tmp_cache["task"] = getCachePid(meminfo, key)
            cache_list.append(tmp_cache)
            if len(cache_list) == 5:
                break
        closeCache = meminfo["Active(file)"] + meminfo["Inactive(file)"] - meminfo["fileCacheMem"]
        tmp_cache = {}
        tmp_cache["cached"] = closeCache
        tmp_cache["file"] = "total cached of close file"
        tmp_cache["task"] = []
        cache_list.append(tmp_cache)
    res["filecacheTop"] = cache_list

    dump2json(res, filepath)

def memgraph_check_memcg(meminfo):
    filename = "/proc/cgroups"
    fd = open(filename,'r')
    ret = fd.read().strip().split("\n")
    fd.close()
    num = 0
    for line in ret:
        if line.find("memory") == -1:
            continue
        values = line.strip().split()
        if len(values) != 4:
          break
        num = int(values[2])
        break
    return num > 1500

def memgraph_check_memfrag(meminfo):
    key = "Normal"
    if meminfo["MemTotal"] < 4*1024*1024:
        key = "DMA32"
    filename = "/proc/buddyinfo"
    fd = open(filename, 'r')
    ret = fd.read().strip().split('\n')
    fd.close()
    frag = []
    for line in ret:
        if line.find(key) == -1:
            continue
        values = line.strip().split()
        frag = values[-8:]
        break
    if len(frag) == 0:
        return False
    total = 0
    for num in frag:
        total += int(num)
    return total < 100

def memgraph_check_event(meminfo):
    event = {}
    util = (meminfo["MemTotal"] - meminfo["MemAvailable"])*100/meminfo["MemTotal"]
    event["util"] = round(util,2)
    ret = memgraph_check_memcg(meminfo)
    event["memcg"] = ret
    ret = memgraph_check_memfrag(meminfo)
    event["memfrag"] = ret
    event["leak"] = False
    if len(meminfo["memleak"]) != 0:
        res = meminfo["memleak"]
        if res["type"] != "No":
            event["leak"] = True
    meminfo["event"] = event

def memgraph_handler_cmd(meminfo, argv):
    global jsonFormat
    filepath = "memgraph.json"
    try:
        opts, args = getopt.getopt(argv,"j:hgfaklc:")
    except getopt.GetoptError:
        print('get opt error')
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print("-g: only show memory usage graph")
            print("-f: show file cache usage detail")
            print("-a: show anon page usage detail")
            print("-k: check kernel memleak")
            print("-l: show memory usage of thread")
            print("-c: show memory usage of cgroup")
            print("-j: output for json file")
            sys.exit()
        elif opt in ("-g"):
            memgraph_graph(meminfo)
        elif opt in ("-j"):
            jsonFormat = True
            filepath = arg
        elif opt in ("-f"):
            memgraph_cache_list(meminfo)
        elif opt in ("-a"):
            memgraph_memory_thread(meminfo, "taskAnon")
        elif opt in ("-k"):
            memgraph_kmemleak_check(meminfo)
        elif opt in ("-l"):
            memgraph_memory_thread(meminfo, "taskMem")
        elif opt in ("-c"):
            depth = int(arg)
            #if depth > 6:
                #depth = 6
            meminfo["cgroupTop"] = [None]*depth
            meminfo["cgroupDepth"] = 0
            memgraph_memory_cgroup(meminfo, depth)
        else:
            print("error args options")
    memgraph_check_event(meminfo)
    memgrapth_output_json(meminfo, filepath)

if __name__ == "__main__":
    meminfo = {}
    meminfo["taskAnon"] = {}
    meminfo["taskMem"] = {}
    meminfo["filepid"] = {}
    meminfo["filecache"] = {}
    meminfo["graph"] = {}
    meminfo["memleak"] = {}
    meminfo["taskInfo"] = {}
    meminfo["cgroupTop"] = {}
    meminfo["cgroupInfo"] = {}
    memgraph_get_meminfo(meminfo)
    if len(sys.argv) == 1:
        memgraph_graph(meminfo)
        sys.exit(0)
    memgraph_handler_cmd(meminfo, sys.argv[1:])
