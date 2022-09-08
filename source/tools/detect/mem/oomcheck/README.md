# 功能说明
检测并分析系统发生过的OOM的原因
# 使用
使用sysak oomcheck -h可查看所有可用的参数
 -h --help     print the help
 -f --dmesg file 后面可以跟需要诊断的日志
 -l --live mode  实时诊断当前系统的系统日志
 -t --time mode  诊断某个时间段的OOM
 -j --output json 以json的格式输出
 -n --# of output results 如果检测到多个OOM，可指定输出的OOM结果的个数
for example:
sysak oomcheck.py
sysak oomcheck.py -t "2021-09-13 15:32:22"
sysak oomcheck.py -t 970665.476522
sysak oomcheck.py -f oom_file.txt
sysak oomcheck.py -f oom_file.txt -t 970665.476522
# 结果分析
##  普通输出
diagnones result: host memory limit ，process:main(2270025) memory usage: 512088KB,oom_score_adj:-999
total oom: 10
total rss: 3450868 KB
task: entry(4683), memory usage: 125148KB
podName: unknow, containerID: 437f8fa5f78fd
host free:16060kB,low:20328kB
type: host, root: limit

diagnones result代表诊断的总结果，下面打印的是诊断的详细信息，包括检测出来的总OOM次数total oom，当前使用的总RSS total rss，
接下来将会打印最后一次OOM使用内存最多的task，和这个task使用的内存memory usage
如果OOM和cgroup有关则会打印相关的podName和containerID，并在最后打印host的相关信息，包括剩余内存host free，内存low水线。
最后将会打印是什么类型的OOM和OOM最主要的原因。type主要有主机和cgroup两种。

root主要类型：

|  root   | 含义  |
|  ----  | ----  |
| limit  | 内存不足 |
| plimit  | 父cgroup内存限制 |
| memleak  | 可能有内存泄漏 |
| fork  | 可能由于多个相同名称的进程占用内存过多 |
| cpuset  | cpuset设置不合理 |
| frag  | 内存碎片导致OOM |
| policy  | mempolicy设置不合理 |
| shmem  | 共享内存导致OOM |

## json格式输出
将会以json格式输出诊断结果，可以供后续解析使用。

{"972970.504516": {"result": "diagnones result: host memory limit ，process:main(2270025) memory usage: 512088KB,oom_score_adj:-999\n", "task": "entry", "containerID": "437f8fa5f78fd", "cg_limit": 0, "root": "limit", "leakusage": 0, "pid": "4683", "task_mem": 125148, "cg_usage": 0, "cg_name": "/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-pod70ee698a_d61c_4876_a54b_130aa53c14d6.slice/cri-containerd-437f8fa5f78fd1bd6c991402d95908d3e505ba57ce200ccb143d98f7889df8dd.scope", "total_oom": 10, "shmem": 0, "total_rss": 3450868, "podName": "unknow", "msg": "total oom: 10\ntotal rss: 3450868 KB\ntask: entry(4683), memory usage: 125148KB\npodName: unknow, containerID: 437f8fa5f78fd\nhost free:16060kB,low:20328kB\ntype: host, root: limit\n", "type": "host", "leaktype": "unknow", "host_free": "16060kB", "host_low": "20328kB"}}
