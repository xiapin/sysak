#!/usr/bin/env python3
import os
import sys
import json
import  getopt
import copy

def get_runtime(podinfo):
    if 'runtime' in podinfo.keys():
        return podinfo['runtime']
    cmd = "which docker 2>&1"
    ret = os.popen(cmd).read()
    if ret.find("no docker in") == -1:
        return "docker"

    cmd = "which crictl 2>&1"
    ret = os.popen(cmd).read()
    if ret.find("no crictl in") == -1:
        return "crictl"
    return "unknow"


def container_init(con):
    con["ns"] = ''
    con["fullid"] = ''
    con["cgroup"] = ''
    con["cparent"] = ''
    con["ino"] = 0
    con["uid"] = ''
    con["podname"] = ''
    con["cname"] = ''
    con["files"] = []
    con["type"] = 'cgroup'
    con["rss"] = 0
    con["cache"] = 0
    con["shmem"] = 0
    
def get_container_info(podinfo, cid, con):
    cmd = podinfo["runtime"] + " inspect " + str(cid)
    res = json.loads(os.popen(cmd).read().strip())
    
    if podinfo['runtime'] == "docker":
        con['fullid'] = res[0]['Id']
        con['type'] = 'docker'
        if 'HostConfig' in res[0] and res[0]['HostConfig'] is not None:
            if 'CgroupParent' in res[0]['HostConfig']:
                con['cparent'] = res[0]['HostConfig']['CgroupParent']
        if 'Config' in res[0] and res[0]['Config'] is not None:
            config = res[0]['Config']
            if 'Labels' in config and config['Labels'] is not None:
                labels = config['Labels']
                if 'io.kubernetes.pod.namespace' in labels and labels['io.kubernetes.pod.namespace'] is not None:
                    con['ns'] = labels['io.kubernetes.pod.namespace']
                    con['uid'] = labels['io.kubernetes.pod.uid']
                    con['podname'] = labels['io.kubernetes.pod.name']
                    con['type'] = 'k8s-docker'
    else:
        con['fullid']  = res['status']['id']
        con['cparent'] = res['info']['runtimeSpec']['linux']['cgroupsPath']
        con['cparent'] = con['cparent'].strip().split(':')[0]
        
        con['ns'] = res['status']['labels']['io.kubernetes.pod.namespace']
        con['uid'] = res['status']['labels']['io.kubernetes.pod.uid']
        con['podname'] = res['status']['labels']['io.kubernetes.pod.name']
        con['type'] = 'k8s'
    if con['podname']  == '':
        con['podname'] = con["cname"]

def get_container_id(podinfo):
    podinfo["runtime"] = get_runtime(podinfo)
    if podinfo["runtime"] == "unknow":
        return False

    podinfo["container"] = {}
    cmd = podinfo['runtime'] + " ps "
    ret = os.popen(cmd).read().split('\n')
    for line in ret:
        if line.find("CONTAINER") != -1:
            continue
        item = line.split()
        if len(item) < 3:
            continue
        con = {}
        container_init(con)
        con["podid"] = item[-1]
        con["cname"] = item[-1]
        if podinfo['runtime'].find('crictl') != -1:
            con["cname"] = item[-3]
        con["id"] = item[0]
        if len(con["cname"]) == 0:
            con["cname"] = con["id"]

        if podinfo['args']['mode'] == 'cid':
            if item[0] != podinfo['args']['cid']:
                continue
        get_container_info(podinfo, item[0], con)
        if is_target_container(podinfo, con) == False:
            continue
        podinfo['container'][item[0]] = con
        build_cgroup_info(podinfo, item[0])
        build_cgroup_mem(podinfo, item[0], con)
        build_tmp_file(podinfo, item[0], con)

def is_target_container(podinfo,con):
    cid = con['id']
    cpodid = con['podid']
    cpodname = con['podname']
    cmdline = podinfo['args']
    mode = cmdline['mode']

    if mode == 'allcgroup':
        return True
    elif mode == 'pod':
        tpodname = cmdline['podname']
        if cpodname.find(tpodname,0, len(tpodname)) != -1:
            return True
    elif mode == 'cid':
        if cmdline['cid'] == cid:
            return True
    return False

def get_file_ino(filename):
    ret = os.stat(filename)
    return ret.st_ino

def get_k8s_path(podinfo, cid):
    pre = "/sys/fs/cgroup/memory"
    k8s = "kubepods.slice"
    qos = ["kubepods-burstable.slice","kubepods-besteffort.slice",'']
    cinfo = podinfo['container'][cid]

    if cinfo['type'] == 'k8s':
        runc = 'cri-containerd'
    elif cinfo['type'] == 'k8s-docker':
        runc = 'docker'
    else:
        return None

    for i in qos:
        podpath = pre + '/' + k8s + '/' + i + '/' + cinfo['cparent'] + '/'
        cpath = runc +'-' + cinfo['fullid'].strip()+".scope"
        if not os.path.exists(podpath+cpath):
            continue
        cinfo['cgroup'] = podpath+cpath
        cinfo['ino'] = get_file_ino(podpath+cpath) 
        return True
    fullpath = cinfo['cparent']
    if fullpath.find(cinfo['fullid']) != -1:
        cpath = pre+ '/' + fullpath
        if os.path.exists(cpath):
            cinfo['cgroup'] = cpath
            cinfo['ino'] = get_file_ino(cpath)
    return None
                    
def get_podman_path(podinfo, cid):
    cinfo = podinfo['container'][cid]
    pre = '/sys/fs/cgroup/memory/machine.slice'
    cpath = pre +'/' +"libpod-" + cinfo['fullid'].strip() + '.scope'
    if not os.path.exists(cpath):
        cinfo['cgroup'] = ''
        cinfo['ino'] = ''
        return None
    cinfo['cgroup'] = cpath
    cinfo['ino'] = get_file_ino(cpath)
    return True

def get_docker_path(podinfo, cid):
    cinfo = podinfo['container'][cid]
    pre = '/sys/fs/cgroup/memory/system.slice'
    cpath = pre +'/' +"docker-" + cinfo['fullid'].strip() + '.scope'
    if not os.path.exists(cpath):
        cinfo['cgroup'] = ''
        cinfo['ino'] = ''
        return None
    cinfo['cgroup'] = cpath
    cinfo['ino'] = get_file_ino(cpath)
    return True

def get_cgroup_path(podinfo, cid):
    cinfo = podinfo['container'][cid]
    return True

def build_cgroup_mem(podinfo, cid, con):
    cinfo = con
    if not os.path.exists(cinfo['cgroup']):
        return True
    stat = cinfo['cgroup'] + '/' + 'memory.stat'
    fd = open(stat, 'r')
    ret = fd.read().strip().split('\n')
    filecache = 0
    mem = {}
    for line in ret:
        item = line.strip().split()
        if len(item) != 2:
            continue
        mem[item[0]] = item[1]
    if 'cache' in mem.keys():
        cinfo['cache'] = int(mem['cache'])/1024
    if 'rss' in mem.keys():
        cinfo['rss'] = int(mem['rss'])/1024
    if 'shmem' in mem.keys():
        cinfo['shmem'] = int(mem['shmem'])/1024

    if 'shmem' not in mem.keys():
        cinfo['shmem'] = int(mem['cache']) - int(mem['inactive_file']) - int(mem['active_file'])
    #print("cache={} rss = {} shmem = {}".format(cinfo['cache'], cinfo['rss'], cinfo['shmem']))

def build_tmp_file(podinfo, cid, con):
    cinfo = con
    if not os.path.exists(cinfo['cgroup']):
        return True
    if cinfo['cache'] < 10:
        return True

    fd = open('/tmp/.memcg.txt','a+')
    fd.write(cinfo['cgroup'] +'\n')
    fd.close()


def dump2json(res,filename):
    jsonStr = json.dumps(res)
    if not os.path.exists(os.path.dirname(filename)):
        os.popen("mkdir -p "+os.path.dirname(filename)).read()
    with open(filename, 'w+') as jsonFile:
        jsonFile.write(jsonStr)

def podmem_to_json(podinfo, cinodes, files):
    output = {}
    output['data'] = {}
    output['subline'] = {}
    cmdline = podinfo['args']
    mode = cmdline['mode']
    if mode == 'cid':
        output['type'] = 'cid'
    elif mode == 'pod':
        output['type'] = 'pod'
    elif mode == 'allcgroup':
        output['type'] = 'cid'

    pod = {}
    for key, cid in podinfo['container'].items():    
        new_cid = {}
        new_cid['sort_file'] = {}
        if len(cid['files']) == 0:
            continue
        new_cid['sort_file'] = cid['files']
        new_cid['id'] = cid['id']
        new_cid['cname'] = cid['cname']
        new_cid['cache'] = cid['cache']
        new_cid['rss'] = cid['rss']
        new_cid['shmem'] = cid['shmem']
        new_cid['podname'] = cid['podname']
        if cid['podname'] not in pod.keys():
            pod[cid['podname']] = []
        pod[cid['podname']].append(new_cid)
    output['data'] = pod
    podinfo['output'] = output

def cgroup_to_json(podinfo, cinodes, files):
    output = {}
    output['data'] = {}
    output['subline'] = {}
    cmdline = podinfo['args']
    mode = cmdline['mode']
    if not (mode=='cgroup' or mode == 'system'):
        return False
 
    if mode == 'system':
        output['type'] = 'system'
        output['subline'] == 'system'
    elif mode == 'cgroup':
        output['type'] = 'cgroup'
    elif mode == 'cid':
        output['type'] = 'cid'
    elif mode == 'pod':
        output['type'] = 'pod'
    elif mode == 'allcgroup':
        output['type'] = 'cid'

    if mode == 'cgroup':
        output['subline'] = cmdline['cgroup'].strip().split('/')[-1]
    output['data'] = {}
    if mode=='cgroup':
        output['data'][output['subline']] = []
        tmp = {}
        tmp['sort_file'] = files
        output['data'][output['subline']].append(tmp)
    elif mode == 'system':
        output['data']['system'] = []
        tmp = {}
        tmp['sort_file'] = files
        output['data']['system'].append(tmp)
    podinfo['output'] = output
    return True

def pod_mem_run(podinfo):
    cmdline = podinfo['args']
    cache_bin = os.getenv("SYSAK_WORK_PATH");
    cache_bin += "/tools/memcache"
    cmd = cache_bin +" -m -f /tmp/.memcg.txt"
    rate = " -r " + str(cmdline['rate']) + " -t " + str(cmdline['top'])
    cmd += rate
    if cmdline['mode'] == 'system':
        cmd += ' -a 1 '
    ret = os.popen(cmd).read().strip().split('\n')
    inodes = {}
    files = []
    if os.path.exists('/tmp/.memcg.txt'):
        os.unlink('/tmp/.memcg.txt')
    for line in ret:
        if line.find('inode=') == -1:
            continue
        item = line.strip().split()
        if len(item)<10:
            continue
        tmp = {}
        tmp['inode'] = int(item[0].split('=')[1])
        tmp['file'] = item[1].split('=')[1]
        tmp['cached'] = int(item[2].split('=')[1])
        tmp['size'] = int(item[3].split('=')[1])
        tmp['active'] = int(item[5].split('=')[1])
        tmp['inactive'] = int(item[6].split('=')[1])
        tmp['shmem'] = int(item[7].split('=')[1])
        tmp['delete'] = int(item[8].split('=')[1])
        tmp['cgcached'] = int(item[9].split('=')[1])
        tmp['dirty'] = int(item[10].split('=')[1])
        cinode = int(item[4].split('=')[1])
        files.append(tmp)
        if not cinode in inodes.keys():
            inodes[cinode] = []
        inodes[cinode].append(tmp)
        
        if cmdline['output'] == 'json':
            continue
        if cmdline['mode'] == 'system':
            print(tmp['file'])
            res = "size: {} cached: {} ".format(tmp['size'], tmp['cached'])
            res +=  "active: {} inactive: {} ".format(tmp['active'],tmp['inactive']) 
            res +=  "shmem: {} delete: {} dirty: {}".format(tmp['shmem'], tmp['delete'], tmp['dirty'])
            print(res)
    mode = podinfo['args']['mode']
    for cid,cinfo in podinfo['container'].items():
        inode = cinfo['ino']
        if not inode  in inodes.keys():
            continue
        cinfo['files'] = inodes[inode]

        if cmdline['output'] == 'json':
            continue
        print('\n')
        if mode == 'cgroup':
            print("cgroup: {}".format(cid))
        elif mode == 'pod':
            print("container name: {} podname:{}".format(cinfo['cname'], cinfo['podname']))
        elif mode == 'cid':
            print("container name {}".format(cinfo['cname']))
        elif mode == 'allcgroup':
            out = "container name:%s"%(cinfo['cname'])
            if cinfo['podname'] != '':
                out += ' podname:%s'%(cinfo['podname'])
            print(out)
        for tmp in cinfo['files']:
            print(tmp['file'])
            res = "size: {} cached: {} cgcached: {} ".format(tmp['size'], tmp['cached'], tmp['cgcached'])
            res +=  "active: {} inactive: {} ".format(tmp['active'],tmp['inactive']) 
            res +=  "shmem: {} delete: {} dirty: {}".format(tmp['shmem'], tmp['delete'], tmp['dirty'])
            print(res)

    if cmdline['output'] != 'json':
        return True
    if cgroup_to_json(podinfo, inodes, files) == False:
        podmem_to_json(podinfo, inodes, files)
    dump2json(podinfo['output'], cmdline['output_file'])
 
def build_cgroup_info(podinfo, cid):
    ctype = podinfo['container'][cid]['type']
    if ctype=='k8s' or ctype == 'k8s-docker':
        get_k8s_path(podinfo, cid)
    elif ctype == 'docker':
        get_docker_path(podinfo, cid)
        if podinfo['container'][cid]['cgroup'] == '':
            get_podman_path(podinfo, cid)
    elif ctype == 'cgroup':
        get_cgroup_path(podinfo, cid)
    else:
        print("get error for cgroup path : {}".format(cid))

def handle_args(podinfo, argv):
    cmdline = {}
    cmdline['rate'] = 1
    cmdline['scan'] = 0
    cmdline['podname'] = ''
    cmdline['cid'] = ''
    cmdline['cgroup'] = ''
    cmdline['mode'] = 'allcgroup'
    cmdline['output'] = 'stdio'
    cmdline['top'] = 10
    try:
        opts, args = getopt.getopt(argv,"hj:r:sap:c:f:t:")
    except getopt.GetoptError:
        print('get opt error')
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print("help for usage:")
            print("-p: analysis pod pagecache(sysak podmem -p ack-node-problem-detector)")
            print("-c: analysis container pagecache(sysak podmem -c bd2146176a5ce)")
            print("-a: analysis all container pagecache(sysak podmem -f /sys/fs/cgroup/memory/system.slice)")
            print("-f: analysis cgroup pagecache(sysak podmem -f /sys/fs/cgroup/memory/system.slice)")
            print("-s: analysis system pagecache(sysak podmem -s )")
            print("-j: dump result to json file (sysak podmem -s -j ./test.json)")
            print("-r: set sample rate ,default set to 1 (sysak podmem -s -r 2)")
            print("-t: output filecache top ,default for top 10 (sysak podmem -s -t 20)")
            sys.exit(2) 
        elif opt == '-r':
            cmdline['rate'] = int(arg)
        elif opt == '-p':
            cmdline['podname'] = arg
            cmdline['mode'] = 'pod'
        elif opt == '-c':
            cmdline['cid'] = arg
            cmdline['mode'] = 'cid'
        elif opt == '-f':
            cmdline['cgroup'] = arg
            cmdline['mode'] = 'cgroup'
        elif opt == '-s':
            cmdline['mode'] = 'system'
        elif opt == '-a':
            cmdline['mode'] = 'allcgroup'
        elif opt == '-t':
            cmdline['top'] = int(arg)
        elif opt == '-j':
            cmdline['output'] = 'json'
            cmdline['output_file'] = arg
    podinfo['args'] = cmdline 

def build_cgroup(podinfo):
    cgroup = podinfo['args']['cgroup']
    con = {}
    container_init(con)
    con['cgroup'] = cgroup
    con['ino'] = get_file_ino(cgroup)
    build_cgroup_mem(podinfo, cgroup, con)
    build_tmp_file(podinfo, cgroup, con)
    podinfo['container'][cgroup] = con

def check_k8s_env(podinfo):

    if not os.path.exists("/proc/kpagecgroup"):
        podinfo['args']['mode'] = 'system'
        return True
    if podinfo['args']['mode'] != 'allcgroup':
        return True

    if len(podinfo["container"]) > 0:
        return True

    podinfo['args']['mode'] = 'system'
    return True
 
if __name__ == "__main__":
    podinfo = {}
    podinfo['container'] = {}
    podinfo['output'] = {}
    handle_args(podinfo, sys.argv[1:])

    if podinfo['args']['mode'] == 'cgroup':
        build_cgroup(podinfo)
    else:
        get_container_id(podinfo)
    check_k8s_env(podinfo)
    pod_mem_run(podinfo)
