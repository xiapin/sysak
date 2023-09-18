# 指标说明

这里记录所有采集到的监控指标说明和来源，方便监控系统集成。

## 通用指标

-------------

### cpu\_total 表

* 对应export 指标 sysak\_proc\_cpu\_total
* 属性标签: mode


| 指标名 | 单位 | 标签说明 | 备注 | 源码路径 |
| :--- | ---: | :---- | :---- | :--- |
| softirq | % | 软中断百分比 |  | collector/proc\_stat.lua |
| user | % | 用户态占用率百分比 |  | collector/proc\_stat.lua |
| guestnice | % | guestnice百分比 |  | collector/proc\_stat.lua |
| guest | % |guest百分比 |  | collector/proc\_stat.lua |
| steal | % |steal百分比 |  | collector/proc\_stat.lua |
| hardirq | % | 硬中断百分比 |  | collector/proc\_stat.lua |
| nice | % | nice百分比 |  | collector/proc\_stat.lua |
| idle | % | idle百分比 |  | collector/proc\_stat.lua |
| sys | % | sys百分比 |  | collector/proc\_stat.lua |
| iowait | % | iowait百分比 |  | collector/proc\_stat.lua |

### cpus 表

* 对应export 指标 sysak\_proc\_cpus
* 属性标签: mode


| 指标名 | 单位 | 标签说明 | 备注 | 源码路径 |
| :--- | ---: | :---- | :---- | :--- |
| cpu_name |  | CPU 名称 |  | collector/proc\_stat.lua |
| softirq | % | 软中断百分比 |  | collector/proc\_stat.lua |
| user | % | 用户态占用率百分比 |  | collector/proc\_stat.lua |
| guestnice | % | guestnice百分比 |  | collector/proc\_stat.lua |
| guest | % |guest百分比 |  | collector/proc\_stat.lua |
| steal | % |steal百分比 |  | collector/proc\_stat.lua |
| hardirq | % | 硬中断百分比 |  | collector/proc\_stat.lua |
| nice | % | nice百分比 |  | collector/proc\_stat.lua |
| idle | % | idle百分比 |  | collector/proc\_stat.lua |
| sys | % | sys百分比 |  | collector/proc\_stat.lua |
| iowait | % | iowait百分比 |  | collector/proc\_stat.lua |


### stat\_counters表

* 对应export 指标 sysak\_proc\_stat\_counters
* 属性标签: counter

| 指标名 | 单位 | 标签说明 | 备注 | 源码路径 |
| :--- | ---: | :---- | :---- | :--- |
| procs_blocked |  | D状态任务数量 |  | collector/proc\_stat.lua |
| processes_forks |  | fork 任务数量 |  | collector/proc\_stat.lua |
| btime |  | 启动时间 |  | collector/proc\_stat.lua |
| procs_running |  |  并行任务数量 |  | collector/proc\_stat.lua |
| ctxt |  | 上下文切换次数 |  | collector/proc\_stat.lua |


### proc\_loadavg 表

* 对应 export 指标： sysak\_proc\_loadavg
* 属性标签：value

| 指标名      | 单位 | 标签说明 | 备注 | 源码路径 |
|:---------| ---: | :---- | :---- | :--- |
| runq | - | rq队列长度 |  | collector/proc\_load.lua |
| load1  | - | load1 |  | collector/proc\_load.lua |
| load5  | - | load5 |  | collector/proc\_load.lua |
| load10  | - | load10 |  | collector/proc\_load.lua |
| plit | - | plit |  | collector/proc\_load.lua |

### meminfo 表

* 对应 export 指标： sysak\_proc\_meminfo
* 属性标签：value

指标说明参考[/proc/meminfo内存文件详解](https://zhuanlan.zhihu.com/p/145524701)

### vmstat 表

* 对应 export 指标： sysak\_proc\_vmstate
* 属性标签：value

指标说明参考[/proc/vmstat输出含义](https://blog.csdn.net/kaka__55/article/details/125236633)

### cgroups 表

* 对应 export 指标： sysak\_cgroups
* 属性标签：value

| 指标名     | 单位 | 标签说明 | 备注 | 源码路径 |
| :---       | --- | :---- | :---- | :--- |
| type       | -  | subsys类型            |  | collector/proc\_cgroups.lua |
| blkio      | 个 | blkio cgroup 数量     |  | collector/proc\_cgroups.lua |
| freezer    | 个 | freezer cgroup数量    |  | collector/proc\_cgroups.lua |
| devices    | 个 | devices cgroup数量    |  | collector/proc\_cgroups.lua |
| hugetlb    | 个 | hugetlb cgroup数量    |  | collector/proc\_cgroups.lua |
| pids       | 个 | blkio cgroup 数量     |  | collector/proc\_cgroups.lua |
| rdma       | 个 | rdma cgroup数量       |  | collector/proc\_cgroups.lua |
| net\_prio   | 个 | net_prio cgroup数量   |  | collector/proc\_cgroups.lua |
| net\_cls    | 个 | net_cls cgroup数量    |  | collector/proc\_cgroups.lua |
| cpu        | 个 | cpu cgroup 数量       |  | collector/proc\_cgroups.lua |
| cpuacct    | 个 | cpuacct cgroup数量    |  | collector/proc\_cgroups.lua |
| perf\_event | 个 | perf_event cgroup数量 |  | collector/proc\_cgroups.lua |
| memory     | 个 | memory cgroup数量     |  | collector/proc\_cgroups.lua |

### interrupts 表

* 对应 export 指标： sysak\_interrupts
* 属性标签：value

| 指标名 | 单位 | 标签说明 | 备注 | 源码路径 |
| :---   | --- | :---- | :---- | :--- |
| cpu      | - | CPU ID       |  | collector/proc\_interrupts.lua |
| 中断名称 | 次 | 中断触发次数 |  | collector/proc\_interrupts.lua |

### mounts 表

* 对应 export 指标： sysak\_fs\_stat
* 属性标签：value

| 指标名 | 单位 | 标签说明 | 备注 | 源码路径 |
| :---   | --- | :---- | :---- | :--- |
| fs     | - | sysfs       |  | collector/proc\_mounts.lua |
| mount  | - | 挂载目录 |  | collector/proc\_mounts.lua |
| f\_bsize  | - | Filesystem block size |  | collector/proc\_mounts.lua |
| f\_blocks | - | Size of fs in f_frsize units |  | collector/proc\_mounts.lua |
| f\_bfree  | - | Number of free blocks |  | collector/proc\_mounts.lua |
| f\_bavail | - | Number of free blocks for unprivileged users |  | collector/proc\_mounts.lua |
| f\_files  | - | Number of inodes |  | collector/proc\_mounts.lua |
| f\_ffree  | - | Number of free inodes |  | collector/proc\_mounts.lua |
| f\_favail | - | Number of free inodes for unprivileged users |  | collector/proc\_mounts.lua |

### softirqs 表

* 对应 export 指标： sysak\_softirqs
* 属性标签：value

| 指标名 | 单位 | 标签说明 | 备注 | 源码路径 |
| :---   | --- | :---- | :---- | :--- |
| cpu     | - | CPU ID       |  | collector/proc\_softirqs.lua |
| HI       | 次 | HI软中断触发次数       |  | collector/proc\_softirqs.lua |
| TIMER    | 次 | TIMER软中断触发次数    |  | collector/proc\_softirqs.lua |
| NET\_TX   | 次 | NET\_TX软中断触发次数   |  | collector/proc\_softirqs.lua |
| NET\_RX   | 次 | NET\_RX软中断触发次数   |  | collector/proc\_softirqs.lua |
| BLOCK    | 次 | BLOCK软中断触发次数    |  | collector/proc\_softirqs.lua |
| IRQ_POLL | 次 | IRQ\_POLL软中断触发次数 |  | collector/proc\_softirqs.lua |
| TASKLET  | 次 | TASKLET软中断触发次数  |  | collector/proc\_softirqs.lua |
| SCHED    | 次 | SCHED软中断触发次数    |  | collector/proc\_softirqs.lua |
| HRTIMER  | 次 | HRTIMER软中断触发次数  |  | collector/proc\_softirqs.lua |
| RCU      | 次 | RCU软中断触发次数      |  | collector/proc\_softirqs.lua |

### self_statm 表

* 统计监控进程的statm信息
* 对应 export 指标： sysak\_self\_statm
* 属性标签：value

| 指标名 | 单位 | 标签说明 | 备注 | 源码路径 |
| :---   | --- | :---- | :---- | :--- |
| size     | Page | total program size |  | collector/proc\_statm.lua |
| resident | Page | resident set size |  | collector/proc\_statm.lua |
| shared   | Page | number of resident shared pages |  | collector/proc\_statm.lua |
| text     | Page | text (code) |  | collector/proc\_statm.lua |
| lib      | Page | library |  | collector/proc\_statm.lua |
| data     | Page | data + stack |  | collector/proc\_statm.lua |
| dt       | Page | dirty pages |  | collector/proc\_statm.lua |

## 网络指标

-----------

### arp

* 对应 export 指标： sysak\_arp
* 属性标签：value

| 指标名 | 单位 | 标签说明 | 备注 | 源码路径 |
| :--- | ---: | :---- | :---- | :--- |
| count | 个 | 网卡名 | 网卡上对应arp表数量  | collector/proc\_arp.lua |

### networks

* 这是网卡流量统计信息，已做差值处理
* 对应 export 指标： sysak\_proc\_networks
* 属性标签：value

| 指标名 | 单位 | 标签说明 | 备注 | 源码路径 |
| :--- | ---: | :---- | :---- | :--- |
| if\_ocompressed | 个 | network\_name 网卡名 | 发送时，设备驱动程序发送或接收的压缩数据包数 | collector/proc\_netdev.lua |
| if\_ocarrier | 个 | network\_name 网卡名 | 发送时，由于carrier错误而丢弃的数据包数 | collector/proc\_netdev.lua |
| if\_ocolls | 个 | network\_name 网卡名 | 发送时，冲突信息包的数目 | collector/proc\_netdev.lua |
| if\_ofifo | 个 | network\_name 网卡名 | 发送时，FIFO缓冲区错误的数量 | collector/proc\_netdev.lua |
| if\_obytes | Byte | network\_name 网卡名 | 发送时，数据的总字节数 | collector/proc\_netdev.lua |
| if\_odrop | 个 | network\_name 网卡名 |  发送时，设备驱动程序丢弃的数据包总数 | collector/proc\_netdev.lua |
| if\_oerrs | 个 | network\_name 网卡名 |  发送时，错误的总数  | collector/proc\_netdev.lua |
| if\_opackets | 个 | network\_name 网卡名 |  发送时，数据包总数 | collector/proc\_netdev.lua |
| if\_icompressed | 个 | network\_name 网卡名 | 接收时，设备驱动程序发送或接收的压缩数据包数 | collector/proc\_netdev.lua |
| if\_ierrs | 个 | network\_name 网卡名 |  接收时，错误的总数 | collector/proc\_netdev.lua |
| if\_ififo | 个 | network\_name 网卡名 |  接收时，FIFO缓冲区错误的数量 | collector/proc\_netdev.lua |
| if\_iframe | 个 | network\_name 网卡名 |  接收时，分组帧错误的数量 | collector/proc\_netdev.lua |
| if\_ipackets | 个 | network\_name 网卡名 |  接收时，数据包总数 | collector/proc\_netdev.lua |
| if\_idrop | 个 | network\_name 网卡名 |  接收时，设备驱动程序丢弃的数据包总数 | collector/proc\_netdev.lua |
| if\_imulticast | 个 | network\_name 网卡名 |  接收时，多播帧数 | collector/proc\_netdev.lua |
| if\_ibytes | 个 | network\_name 网卡名 |  接收时，数据字节总数 | collector/proc\_netdev.lua |

### pkt_status

* 对应 export 指标： sysak\_proc\_pkt\_status
* 属性标签：counter
* 这里统计所有包状态，详细可以通过 pkt\_logs 获取

| 指标名 | 单位 | 标签说明 | 备注 | 源码路径 |
| :--- | ---: | :---- | :---- | :--- |
| abort | 次 |  | 协议栈断言失效次数 | collector/proc\_snmp\_stat.lua |
| overflow | 次 |  | 协议栈溢出次数 | collector/proc\_snmp\_stat.lua |
| err | 次 |  | 协议栈错误次数 | collector/proc\_snmp\_stat.lua |
| paws | 次 |  | 协议栈PAWS回绕次数 | collector/proc\_snmp\_stat.lua |
| fail	 | 次 |  | 协议栈failure次数 | collector/proc\_snmp\_stat.lua |
| retrans | 次 |  | 协议栈溢出次数 | collector/proc\_snmp\_stat.lua |
| drop | 次 |  | 协议栈丢包次数 | collector/proc\_snmp\_stat.lua |

### sock_stat 

* 对应 export 指标： sysak\_sock\_stat
* 属性标签：value

统计所有包状态。[参考连接](https://developer.aliyun.com/article/484451)

|  指标名           | 单位 | 标签说明 | 备注 | 源码路径 |
|:---------------| ---: | :---- | :---- | :--- |
| frag\_inuse    | 个 |  | 使用的IP段数量 | collector/proc\_sockstat.lua |
| frag\_memory   | 页 |  | IP段使用内存数量 | collector/proc\_sockstat.lua |
| udplite\_inuse | 个 |  | udplite 使用量 | collector/proc\_sockstat.lua |
| udp\_mem       | 页 |  | udp socket 内存使用量，含收发缓冲区队列 | collector/proc\_sockstat.lua |
| udp\_inuse     | 个 |  | udp 使用量 | collector/proc\_sockstat.lua |
| tcp\_mem       | 页 |  | udp socket 内存使用量，含收发缓冲区队列 | collector/proc\_sockstat.lua |
| tcp\_alloc     | 个 |  | TCP socket 申请总数 | collector/proc\_sockstat.lua |
| tcp\_tw        | 个 |  | TCP time wait socket 总数 | collector/proc\_sockstat.lua |
| tcp\_orphan    | 个 |  | TCP ophan socket 总数 | collector/proc\_sockstat.lua |
| tcp\_inuse     | 个 |  | TCP 常规 socket 总数 | collector/proc\_sockstat.lua |
| raw\_inuse     | 个 |  | raw socket 使用量 | collector/proc\_sockstat.lua |
| sockets\_used  | 个 |  | 总socket 使用量 | collector/proc\_sockstat.lua |


### softnets

* 对应 export 指标： sysak\_softnets
* 属性标签：value

| 指标名 | 单位 | 标签说明 | 备注 | 源码路径 |
| :--- | ---: | :---- | :---- | :--- |
| packet\_process | 个 | cpu，对应CPU号 | 所在核收包个数 | collector/proc\_softnet\_stat.lua |
| packet\_drop | 个 | cpu，对应CPU号 | 所在核丢包个数 | collector/proc\_softnet\_stat.lua |
| cpu\_collision | 个 | cpu，对应CPU号 | 是为了发送包而获取锁的时候有冲突的次数. | collector/proc\_softnet\_stat.lua |
| received\_rps | 个 | cpu，对应CPU号 | 这个 CPU 被其他 CPU 唤醒去收包的次数. | collector/proc\_softnet\_stat.lua |
| time\_squeeze | 个 | cpu，对应CPU号 | 函数 net\_rx\_action 调用次数. | collector/proc\_softnet\_stat.lua |
| flow\_limit\_count | 个 | cpu，对应CPU号 | 达到 flow limit 的次数. | collector/proc\_softnet\_stat.lua |


### con_net_stat 表

| 指标名       | 单位 | 标签说明                     | 备注 | 源码路径                                   |
| :----------- | ---- | :--------------------------- | :--- | :----------------------------------------- |
| net_rx_bytes     | byte | 容器对应网口接收字节数   | interface对应容器中的网口（如eth0）| collector/container/con_net_stat.lua |
| net_rx_packets | packet   | 容器对应网口接收数据包数                |      | collector/container/con_net_stat.lua  |
| net_rx_dropped | packet   | 容器对应网口接收端丢包数 |      | collector/container/con_net_stat.lua  |
| net_tx_bytes     | byte | 容器对应网口发送字节数               |      | collector/container/con_net_stat.lua |
| net_tx_packets | packet   | 容器对应网口发送数据包数                |      | collector/container/con_net_stat.lua  |
| net_tx_dropped | packet   | 容器对应网口发送端丢包数 |      | collector/container/con_net_stat.lua  |


### cgroups 表

* 对应 export 指标： sysak\_cgroups
* 属性标签：value

| 指标名     | 单位 | 标签说明 | 备注 | 源码路径 |
| :---       | --- | :---- | :---- | :--- |
| type       | -  | subsys类型            |  | collector/proc\_cgroups.lua |
| blkio      | 个 | blkio cgroup 数量     |  | collector/proc\_cgroups.lua |
| freezer    | 个 | freezer cgroup数量    |  | collector/proc\_cgroups.lua |
| devices    | 个 | devices cgroup数量    |  | collector/proc\_cgroups.lua |
| hugetlb    | 个 | hugetlb cgroup数量    |  | collector/proc\_cgroups.lua |
| pids       | 个 | blkio cgroup 数量     |  | collector/proc\_cgroups.lua |
| rdma       | 个 | rdma cgroup数量       |  | collector/proc\_cgroups.lua |
| net\_prio   | 个 | net_prio cgroup数量   |  | collector/proc\_cgroups.lua |
| net\_cls    | 个 | net_cls cgroup数量    |  | collector/proc\_cgroups.lua |
| cpu        | 个 | cpu cgroup 数量       |  | collector/proc\_cgroups.lua |
| cpuacct    | 个 | cpuacct cgroup数量    |  | collector/proc\_cgroups.lua |
| perf\_event | 个 | perf_event cgroup数量 |  | collector/proc\_cgroups.lua |
| memory     | 个 | memory cgroup数量     |  | collector/proc\_cgroups.lua |


## IO指标

-------------

### IOMonIndForDisksIO 表 
统计磁盘级IO信息

|  指标名         | 单位 | 标签说明 | 备注 | 源码路径 |
|:---------------| ---: | :---- | :---- | :--- |
| ioburstCnt     | 次 | 每分钟io burst（IO压力突发增大）次数 | - | ../ioMonitor/ioMon/ioMonitorClass.py |
| iodelayCnt     | 次 | 每分钟io延迟高次数 | - | ../ioMonitor/ioMon/ioMonitorClass.py |
| iohangCnt      | 次 | 每分钟io hang次数 | - | ../ioMonitor/ioMon/ioMonitorClass.py |
| bps            | kB | 磁盘bps | - | ../ioMonitor/ioMon/ioMonitorClass.py |
| iops           | 个 | 磁盘iops | - | ../ioMonitor/ioMon/ioMonitorClass.py |
| qusize         | 个 |  未完成io数 | - | ../ioMonitor/ioMon/ioMonitorClass.py |
| util           | 占比 | IO繁忙度 | - | ../ioMonitor/ioMon/ioMonitorClass.py |
| await          | ms |  平均每个IO的延迟 | - | ../ioMonitor/ioMon/ioMonitorClass.py |

### disks表
* 对应 export 指标： sysom\_proc\_disks
* 属性标签：counter

| 指标名 | 单位 | 标签说明 | 备注 | 源码路径 |
| :---   | --- | :---- | :---- | :--- |
| disk_name | - | 磁盘或分区名 | - | collector/proc\_diskstats.lua |
| busy | % | 该磁盘非空闲的时间比例 | - | collector/proc\_diskstats.lua |
| inflight | 个 | 正在进行中的IO请求个数 | - | collector/proc\_diskstats.lua |
| xfers | 次 | 读写总次数 | - | collector/proc\_diskstats.lua |
| bsize | B | 平均每次IO的字节数 | - | collector/proc\_diskstats.lua |
| reads | 次 | 读请求次数 | - | collector/proc\_diskstats.lua |
| writes | 次 | 写请求次数 | - | collector/proc\_diskstats.lua |
| rmerge | 次 | 合并后的读请求数 | - | collector/proc\_diskstats.lua |
| wmerge | 次 | 合并后的写请求数 | - | collector/proc\_diskstats.lua |
| rkb | KB | 写的总流量大小 | - | collector/proc\_diskstats.lua |
| wkb | KB | 写的总流量大小 | - | collector/proc\_diskstats.lua |
| rmsec | ms | 所有读请求总耗时 | - | collector/proc\_diskstats.lua |
| wmsec | ms | 所有写请求总耗时 | - | collector/proc\_diskstats.lua |


### IOMonIndForSystemIO 表 
统计系统IO异常

|  指标名         | 单位 | 标签说明 | 备注 | 源码路径 |
|:---------------| ---: | :---- | :---- | :--- |
| iowait         | 占比 |  系统等待IO的占比 | - | ../ioMonitor/ioMon/ioMonitorClass.py |
| iowaithighCnt  | 次 |  每分钟iowait高次数 | - | ../ioMonitor/ioMon/ioMonitorClass.py |


## 混部指标

-----------

### cg_cpu_stat 表

* 对应 export 指标： sysom\_container\_cpu\_stat
* 属性标签：value

| 指标名 | 单位 | 标签说明 | 备注 | 源码路径 |
| :---   | --- | :---- | :---- | :--- |
| nr_throttled | - | total throttled number |  | collector/container/cg\_cpu\_stat.lua | 
| throttled_time | ms | total throttled time |  | collector/container/cg\_cpu\_stat.lua | 
| nr_periods | - | total period number |  | collector/container/cg\_cpu\_stat.lua |

### cg_proc_stat 表

* 对应 export 指标： sysom\_container\_proc\_stat
* 属性标签：value

| 指标名 | 单位 | 标签说明 | 备注 | 源码路径 |
| :---   | --- | :---- | :---- | :--- |
| user      | % | usr cpu util |  | collector/container/cg\_cpuacct\_proc\_stat.lua | 
| nice      | % | nice cpu util |  | collector/container/cg\_cpuacct\_proc\_stat.lua | 
| system    | % | system cpu util |  | collector/container/cg\_cpuacct\_proc\_stat.lua | 
| idle      | % | idl cpu util |  | collector/container/cg\_cpuacct\_proc\_stat.lua | 
| iowait    | % | iowait cpu util |  | collector/container/cg\_cpuacct\_proc\_stat.lua | 
| irq       | % | irq cpu util |  | collector/container/cg\_cpuacct\_proc\_stat.lua | 
| softirq   | % | softirq cpu util |  | collector/container/cg\_cpuacct\_proc\_stat.lua | 
| steal     | % | steal cpu util |  | collector/container/cg\_cpuacct\_proc\_stat.lua | 
| guest     | % | guest cpu util |  | collector/container/cg\_cpuacct\_proc\_stat.lua |
| load1min  | - | load of 1min |  | collector/container/cg\_cpuacct\_proc\_stat.lua |
| load5min  | - | load of 5min |  | collector/container/cg\_cpuacct\_proc\_stat.lua |
| load15min  | - | load of 15min |  | collector/container/cg\_cpuacct\_proc\_stat.lua |
| r_load1min  | - | running load of 1min |  | collector/container/cg\_cpuacct\_proc\_stat.lua |
| r_load5min  | - | running load of 5min |  | collector/container/cg\_cpuacct\_proc\_stat.lua |
| r_load15min  | - | running  load of 15min |  | collector/container/cg\_cpuacct\_proc\_stat.lua |
| nr_running  | - | number of runable tasks |  | collector/container/cg\_cpuacct\_proc\_stat.lua |
| nr_uninterruptible  | - | number of deep sleep tasks |  | collector/container/cg\_cpuacct\_proc\_stat.lua |
 
### cg_memfail_cnt 表

* 对应 export 指标： sysom\_container\_memfail\_cnt
* 属性标签：value

| 指标名 | 单位 | 标签说明 | 备注 | 源码路径 |
| :---   | --- | :---- | :---- | :--- |
| fail_cnt | - | number of mem fail counts |  | collector/container/cg\_memory\_fail\_cnt.lua | 
 
### cg_memdrcm_latency 表

* 对应 export 指标： sysom\_container\_memdrcm\_latency
* 属性标签：value

This table show the hist of the latency of direct memory reclamation

| 指标名 | 单位 | 标签说明 | 备注 | 源码路径 |
| :---   | --- | :---- | :---- | :--- |
| memDrcm_lat_1to5ms | - | times 1to5ms |  | collector/container/cg\_memory\_drcm\_latency.lua | 
| memDrcm_lat_5to10ms | - | times 5to10ms |  | collector/container/cg\_memory\_drcm\_latency.lua | 
| memDrcm_lat_10to100ms | - | times 10to100ms |  | collector/container/cg\_memory\_drcm\_latency.lua | 
| memDrcm_lat_100to500ms | - | times 100to500ms |  | collector/container/cg\_memory\_drcm\_latency.lua | 
| memDrcm_lat_500to1000ms | - | times 500msto1s |  | collector/container/cg\_memory\_drcm\_latency.lua | 
| memDrcm_lat_1000ms | - | times more than 1s |  | collector/container/cg\_memory\_drcm\_latency.lua | 

### cg_blkio_stat 表

* 对应 export 指标： sysom\_container\_blkio\_stat
* 属性标签：value

| 指标名 | 单位 | 标签说明 | 备注 | 源码路径 |
| :---   | --- | :---- | :---- | :--- |
| writes_service_bytes | byte | 容器中写字节数 | device对应容器进行IO的块设备名  | collector/container/cg\_blkio\_stat.lua | 
| reads_service_bytes | byte | 容器中读字节数 |  | collector/container/cg\_blkio\_stat.lua | 
| total_service_bytes | byte | 容器所有IO操作（read, write, sync, async）的字节数 |  | collector/container/cg\_blkio\_stat.lua |
| writes_serviced | I/Os | 容器中写IO个数 |  | collector/container/cg\_blkio\_stat.lua | 
| reads_serviced | I/Os| 容器中读IO个数|  | collector/container/cg\_blkio\_stat.lua | 
| total_serviced | I/Os | 容器中所有IO操作（read, write, sync, async）的个数 |  | collector/container/cg\_blkio\_stat.lua |
| writes_bytes_queued | byte |  容器中写排队的IO字节数 | | collector/container/cg\_blkio\_stat.lua | 
| reads_bytes_queued | byte | 容器中读排队的IO字节数|  | collector/container/cg\_blkio\_stat.lua | 
| total_bytes_queued | byte | 容器所有IO操作（read, write, sync, async）的排队字节数 |  | collector/container/cg\_blkio\_stat.lua |
| writes_io_queued | I/Os |  容器中写排队IO个数 || collector/container/cg\_blkio\_stat.lua | 
| reads_io_queued | I/Os | 容器中读排队IO个数|  | collector/container/cg\_blkio\_stat.lua | 
| total_io_queued | I/Os | 容器中对所有IO操作（read, write, sync, async）排队个数 |  | collector/container/cg\_blkio\_stat.lua |
| writes_wait_time | ns |  容器中写I/O在调度队列中等待的时间 || collector/container/cg\_blkio\_stat.lua | 
| reads_wait_time | ns | 容器中读I/O在调度队列中等待的时间|  | collector/container/cg\_blkio\_stat.lua | 
| total_wait_time | ns | 容器中对所有IO操作（read, write, sync, async）在调度队列中等待的时间 |  | collector/container/cg\_blkio\_stat.lua |

### cg_memmcmp_latency 表

* 对应 export 指标： sysom\_container\_memmcmp\_latency
* 属性标签：value

This table show the hist of the latency of direct memory compaction

| 指标名 | 单位 | 标签说明 | 备注 | 源码路径 |
| :---   | --- | :---- | :---- | :--- |
| memDcmp_lat_1to5ms | - | times 1to5ms |  | collector/container/cg\_memory\_dcmp\_latency.lua | 
| memDcmp_lat_5to10ms | - | times 5to10ms |  | collector/container/cg\_memory\_dcmp\_latency.lua | 
| memDcmp_lat_10to100ms | - | times 10to100ms |  | collector/container/cg\_memory\_dcmp\_latency.lua | 
| memDcmp_lat_100to500ms | - | times 100to500ms |  | collector/container/cg\_memory\_dcmp\_latency.lua | 
| memDcmp_lat_500to1000ms | - | times 500msto1s |  | collector/container/cg\_memory\_dcmp\_latency.lua | 
| memDcmp_lat_1000ms | - | times more than 1s |  | collector/container/cg\_memory\_dcmp\_latency.lua | 
 
### pmu_events 表

* 对应 export 指标： sysom\_pmu\_events
* 属性标签：value

| 指标名 | 单位 | 标签说明 | 备注 | 源码路径 |
| :---   | --- | :---- | :---- | :--- |
| cycles | - | cycles |  | collector/plugin/pmu_events/pmu\_events.c  | 
| ins | - | instructions |  | collector/plugin/pmu_events/pmu\_events.c  | 
| IPC | - | instructions per cycles |  | collector/plugin/pmu_events/pmu\_events.c  | 
| CPI | - | cycles per instructions |  | collector/plugin/pmu_events/pmu\_events.c  | 
| llcStore | - | llc stroe hits counts |  | collector/plugin/pmu_events/pmu\_events.c  | 
| llcStoreMis | - | llc stroe miss counts |  | collector/plugin/pmu_events/pmu\_events.c  | 
| llcLoad | - | llc load hits counts |  | collector/plugin/pmu_events/pmu\_events.c  | 
| llcLoadMis | - | llc load miss counts |  | collector/plugin/pmu_events/pmu\_events.c  | 
| l3LoadMisRate | - | llc load miss rate |  | collector/plugin/pmu_events/pmu\_events.c  | 
| l3StoreMisRate | - | llc store miss rate |  | collector/plugin/pmu_events/pmu\_events.c  | 
| l3MisRate | - | llc miss rate |  | collector/plugin/pmu_events/pmu\_events.c  | 
| MPI | - | llc miss per instructions |  | collector/plugin/pmu_events/pmu\_events.c  | 


### imc_node_event 表

* 对应 export 指标： sysom\_imc\_event\_node
* 属性标签：value

| 指标名 | 单位 | 标签说明              | 备注   | 源码路径                                 |
| :----- | ---- | :-------------------- | :----- | :--------------------------------------- |
| rlat   | ns   | imc read ddr latency  | node级 | collector/plugin/uncore_imc/uncore_imc.c |
| wlat   | ns   | imc write ddr latency | node级 | collector/plugin/uncore_imc/uncore_imc.c |
| avglat | ns   | imc avg ddr latency   | node级 | collector/plugin/uncore_imc/uncore_imc.c |

### imc_socket_event 表

* 对应 export 指标： sysom\_imc\_event\_socket
* 属性标签：value

| 指标名 | 单位 | 标签说明              | 备注     | 源码路径                                 |
| :----- | ---- | :-------------------- | :------- | :--------------------------------------- |
| rlat   | ns   | imc read ddr latency  | socket级 | collector/plugin/uncore_imc/uncore_imc.c |
| wlat   | ns   | imc write ddr latency | socket级 | collector/plugin/uncore_imc/uncore_imc.c |
| avglat | ns   | imc avg ddr latency   | socket级 | collector/plugin/uncore_imc/uncore_imc.c |
| bw_rd  | byte | imc read bandwidth    | socket级 | collector/plugin/uncore_imc/uncore_imc.c |
| bw_wr  | byte | imc write bandwidth   | socket级 | collector/plugin/uncore_imc/uncore_imc.c |

### imc_channel_event 表

* 对应 export 指标： sysom\_imc\_channel\_event
* 属性标签：value

| 指标名 | 单位 | 标签说明              | 备注      | 源码路径                                 |
| :----- | ---- | :-------------------- | :-------- | :--------------------------------------- |
| rlat   | ns   | imc read ddr latency  | channel级 | collector/plugin/uncore_imc/uncore_imc.c |
| wlat   | ns   | imc write ddr latency | channel级 | collector/plugin/uncore_imc/uncore_imc.c |
| avglat | ns   | imc avg ddr latency   | channel级 | collector/plugin/uncore_imc/uncore_imc.c |
| bw_rd  | byte | imc read bandwidth    | channel级 | collector/plugin/uncore_imc/uncore_imc.c |
| bw_wr  | byte | imc write bandwidth   | channel级 | collector/plugin/uncore_imc/uncore_imc.c |


### rdt_usage 表

* 对应 export 指标： sysom\_rdt\_usage
* 属性标签：value

| 指标名       | 单位 | 标签说明                     | 备注 | 源码路径                                   |
| :----------- | ---- | :--------------------------- | :--- | :----------------------------------------- |
| llc_occ      | byte | llc occupancy                |      | collector/rdt/plugin/rdt_llc_occupancy.lua |
| local_mem_bw | MB   | 本地内存带宽                 |      | collector/rdt/plugin/rdt_local_mem_bw.lua  |
| total_mem_bw | MB   | Remote内存带宽+Local内存带宽 |      | collector/rdt/plugin/rdt_total_mem_bw.lua  |

### rdt_alloc_policy 表

* 对应 export 指标： sysom\_rdt\_alloc\_policy
* 属性标签：value

| 指标名 | 单位 | 标签说明         | 备注                   | 源码路径                          |
| :----- | ---- | :--------------- | :--------------------- | :-------------------------------- |
| MB     |      | 内存带宽分配比例 | MB（Memory Bandwidth） | collector/rdt/plugin/rdt_size.lua |
| L3     | byte | llc分配size      |                        | collector/rdt/plugin/rdt_size.lua |

