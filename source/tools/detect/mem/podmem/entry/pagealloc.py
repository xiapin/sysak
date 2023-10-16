import os
import sys
import getopt
import json
import ctypes
import time
import subprocess
proc_fs="/mnt/host/proc/"
sys_fs="/mnt/host/sys/"
pods_fs="/mnt/host/var/lib/kubelet/pods/"
rootfs = "/mnt/host/"
def os_cmd(cmd):
    ret = os.popen(cmd).read().split("\n")
    return ret

def get_host_fs():
    global proc_fs
    global sys_fs
    global pods_fs
    global rootfs
    if os.path.exists(proc_fs):
        return
    proc_fs="/proc/"
    sys_fs="/sys/" 
    pods_fs="/var/lib/kubelet/pods/"
    rootfs = "/"

def get_runtime_sock(meminfo):
    global rootfs
    meminfo["runtime"] = "docker"
    meminfo["runtime_sock"] = "/var/run/docker.sock"
    runtime_sock = ""
    sock=["var/run/docker.sock","run/podman/podman.sock", "run/containerd/containerd.sock", "var/run/dockershim.sock"] 
    for runtime in sock:
        runtime_sock = rootfs + runtime
        if os.path.exists(runtime_sock):
            meminfo["runtime_sock"] = runtime_sock
            if runtime_sock.find("docker.sock") == -1 or runtime_sock.find("podman.sock") == -1:
                meminfo["runtime"] = "crictl"
            return runtime_sock
    print("get docker runtime error")
    return "" 
    
def get_local_ip(line):
    if line.find("*:") != -1:
        return "local"
    if line.find(".") == -1:
        return "unknow"

    ip = line.strip().split()
    if len(ip) < 4:
        return "unknow"
    return ip[4]

def set_ns(pid):
    global proc_fs
    pid_ns =  proc_fs+ pid + "/ns/net"
    if not os.path.exists(pid_ns):
        return
    libc = ctypes.CDLL('libc.so.6')
    netfd = os.open(pid_ns, os.O_RDONLY)
    libc.setns(netfd, 0)
    os.close(netfd)
 
def get_task(line):
    info=["unknow","0","local"]
    local_ip = get_local_ip(line)
    info[2] = local_ip
    if line.find("users") == -1:
        return info

    start = line.find("(")
    if start == -1:
        return info

    end = line.find(")")
    if end == -1:
        return info

    task = line[start+3:end].strip().replace('\"','')
    if len(task) < 2:
        return info
    proc = task.strip().split(',')
    if len(proc) < 2:
        return info
    info[0] = proc[0]
    info[1] = proc[1].strip().split("pid=")[1]
    return info

def pid_docker(pid):
    global proc_fs
    cmd_cgroup = "cat " + proc_fs + pid+"/cgroup 2>/dev/null | grep memory:"
    ret = os_cmd(cmd_cgroup)[0]

    # task in cgroup not docker cgroup
    docker_id = 'task'
    if (ret.find("kubepods") == -1) and (ret.find("docker-") == -1):
        return "task"
    if ret.find("docker-") != -1:
        docker_id = ret.split("docker-")[1][:8]
    elif ret.find("cri-containerd-") != -1:
        docker_id = ret.split("cri-containerd-")[1][:8]
    else:
        docker_id = ret.split("/")[-1][:8]
    print("docker id: {} ret:{}".format(docker_id, ret))
    return docker_id

def pod_uuid(pid):
    global proc_fs
    cmd_cgroup = "cat " +proc_fs+pid+"/cgroup | grep memory:"
    ret = os_cmd(cmd_cgroup)[0]
    #ret = "4:memory:/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-pod25847a2d_4780_4391_8163_563bed2bc696.slice/cri-containerd-1889de69c78374331e5d5bd436b5ecd066f4cd7750e8413b0f76372c936146fe.scope"
    ret = ret.split("/")
    if len(ret) < 3:
        return "unknow"
    cid = pid_docker(ret[-1])
    ret = ret[-2]
    if ret.find("kubepods") != -1:
        ret = ret.split("kubepods")[1]
    if ret.find("pod") == -1:
        return cid
    uuid = ret.split("pod")[1]
    if uuid.find(".slice") != -1:
        uuid = uuid.split(".slice")[0]
    uuid = uuid.replace("_","-")
    return uuid

def pod_id(pid, task):
    docker_id = pid_docker(pid)
    if docker_id == "task":
        docker_id = task
    return docker_id

def get_container_info(meminfo, cid, task):
    # cid not docker id
    if cid == task:
        return  task
    if meminfo["runtime"] == "docker":
        res = get_container_inspect(meminfo, cid)
    else :
        res = get_crictl_inspect(meminfo, cid)
    res = get_info(meminfo, res, cid)
    if res == "unknow":
        res = task
    print("get_container_info: podname {}".format(res))
    return res
 
def get_container_inspect(meminfo, cid):
    runtime = meminfo["runtime"]
    runtime_sock = meminfo["runtime_sock"]
    if runtime != "docker":
        return {}
    cmd = "curl --silent -XGET --unix-socket " + runtime_sock + " http://localhost/containers/"
    cmd += cid.strip() + "/json" + " 2>/dev/null "
    print(cmd)
    res = os.popen(cmd).read().strip()
    return res

def get_crictl_inspect(meminfo, cid):
    runtime = meminfo["runtime"]
    sock = meminfo["runtime_sock"]
    if runtime != "crictl":
        return {}
    cmd = runtime + " -r " +  sock + " inspect " + cid + " 2>/dev/null " 
    res = os.popen(cmd).read().strip()
    return res

def get_info(meminfo, result,cid):
    podname = cid
    cname = cid
    podns = cid
    if len(result) < 200:
        return "unknow"
    res = json.loads(result)
    if isinstance(res,list):
        res = res[0]
    if 'Config' in res:
        config = res['Config']
        if 'Labels' in config:
            labels = config['Labels']
            if 'io.kubernetes.pod.namespace' in labels:
                podname= labels['io.kubernetes.pod.name']
            if 'io.kubernetes.container.name' in labels:
                cname = labels['io.kubernetes.container.name']
            if 'io.kubernetes.pod.namespace' in labels:
                podns = labels['io.kubernetes.pod.namespace']
        elif "Name" in res:
            cname= res["Name"]
            podname = res["Name"]
    elif "status" in res :
        podname = res['status']['labels']['io.kubernetes.pod.name']
        cname = res['status']['labels']['io.kubernetes.container.name']
        podns = res['status']['labels']['io.kubernetes.pod.namespace']
    if podname not in meminfo["podinfo"].keys():
        meminfo["podinfo"][podname] = {}
        meminfo["podinfo"][podname]["podname"] = podname
        meminfo["podinfo"][podname]["podns"] = podns
        meminfo["podinfo"][podname]["rxmem"] = 0
        meminfo["podinfo"][podname]["txmem"] = 0
    return podname

def pagemem_scan(meminfo, ns):
    try:
        pagemem_check(meminfo, ns)
    except Exception :
        import traceback
        traceback.print_exc()
        pass
 
def pagemem_check(meminfo,ns):
    memPod = {}
    memTask = {}
    tx_mem = 0
    rx_mem = 0
    global proc_fs 
    for net, pid in ns.items():
        if pid == "self":
            pid = "1"
        set_ns(pid)
        env = dict(os.environ)
        env['PROC_ROOT'] = proc_fs
        p = subprocess.Popen("ss -pna", shell=True, env=env, stdout=subprocess.PIPE)
        ret = p.stdout.readlines()
        #ret = os_cmd("ss -anp")
        idx = 0
        for idx in range(len(ret)):
            line = ret[idx].decode("utf-8")
            if line.find(":") == -1:
                continue
            if line.find("Recv-Q") != -1:
                continue
            line_list = line.strip().split()
            if len(line_list) < 4:
                continue
            proto = line_list[0]
            if proto != "tcp" and proto != "udp" and proto != "raw" and  proto != "tcp6":
                continue
            info = get_task(line)
            task = info[0]
            pid = info[1]
            task_pid = task+"-"+pid
            rx = int(line_list[2])
            if line.find("LISTEN") >= 0:
                tx = 0
            else:
                tx = int(line_list[3])
            rx_mem += rx
            tx_mem += tx
            if task_pid not in memTask.keys():
                memTask[task_pid] = pod_id(info[1],task)
            cid = memTask[task_pid]
            if cid not in memPod.keys():
                podname = get_container_info(meminfo, cid, task)
                memPod[cid] = podname
            podname = memPod[cid]
            if rx < 1024 and tx < 1024 and podname == task:
                continue
            #print("podname:{} task:{} pid: {} rx:{}".format(podname, task, pid,rx))
            if podname not in meminfo["podinfo"].keys():
                meminfo["podinfo"][podname] = {}
                meminfo["podinfo"][podname]["rxmem"] = 0
                meminfo["podinfo"][podname]["txmem"] = 0
                meminfo["podinfo"][podname]["podname"] = podname
                meminfo["podinfo"][podname]["podns"] = podname
            meminfo["podinfo"][podname]["rxmem"] += rx
            meminfo["podinfo"][podname]["txmem"] += tx
        set_ns("1")
    total_rx = (rx_mem) / 1024
    total_tx = (tx_mem) / 1024
    meminfo["rx_queue"] = rx_mem/1024
    meminfo["tx_queue"] = tx_mem/1024
    meminfo["task"] = memPod
    print("total Recv-Q: {:.2f}K".format(rx_mem/1024))
    print("total Send-Q: {:.2f}K".format(tx_mem/1024))
    for podname,value in meminfo["podinfo"].items():
        print("Recv-Q: {:.2f}K Send-Q: {:.2f}K podname:{} podns: {}".format(value["rxmem"]/1024, value["txmem"]/1024, value["podname"],value["podns"]))
    return meminfo


def scan_all_namespace(ns):
    global proc_fs
    root = proc_fs
    try:
        for proc in os.listdir(root):
            if not os.path.exists(root + proc + "/comm"):
                continue
            procNs = root + proc + "/ns/net"
            try:
                if not os.path.exists(procNs):
                    continue
                link = os.readlink(procNs)
                if link.find("net") == -1:
                    continue
                net = link.strip().split("[")
                if len(net) < 2:
                    continue
                inode = net[1][:-1].strip()
                if inode not in ns:
                    ns[inode] = proc.strip()
                if not os.path.exists(proc_fs+ns[inode]+"/comm"):
                    ns[inode] = proc.strip()
            except Exception:
                import traceback
                traceback.print_exc()
                pass
    except Exception :
        import traceback
        traceback.print_exc()
        pass
    return ns

def page_mem():
    meminfo = {}
    ns = {}
    get_host_fs()
    meminfo["podinfo"] = {}
    get_runtime_sock(meminfo)
    scan_all_namespace(ns)
    pagemem_check(meminfo,ns)
    return meminfo
 
if __name__ == "__main__":
    meminfo = {}
    meminfo["podinfo"] = {}
    ns = {}
    get_host_fs()
    get_runtime_sock(meminfo)
    scan_all_namespace(ns)
    pagemem_scan(meminfo,ns)
