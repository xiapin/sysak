# 指标说明

这里记录所有采集到的监控指标说明和来源，方便监控系统集成。

## 通用指标

-------------

### uptime 表 

| 指标名 | 单位 | 标签说明 | 备注 | 源码路径 |
| :--- | ---: | :---- | :---- | :--- |
| uptime | 秒 | 从系统启动到现在的时间 |  | collector/proc\_uptime.lua |
| idletime | 秒 | 系统总空闲的时间 |  | collector/proc\_uptime.lua |
| stamp | 秒 | 系统时间戳 | unix 时间  | collector/proc\_uptime.lua |

### uname 表

每小时获取一次

| 指标名      | 单位 | 标签说明 | 备注 | 源码路径 |
|:---------| ---: | :---- | :---- | :--- |
| nodename | - | uname -r |  | collector/proc\_uptime.lua |
| version  | - | uname -r |  | collector/proc\_uptime.lua |
| release  | - | uname -r |  | collector/proc\_uptime.lua |
| machine  | - | uname -r |  | collector/proc\_uptime.lua |
| sysname | - | uname -r |  | collector/proc\_uptime.lua |

## 网络指标

-----------

### arp

| 指标名 | 单位 | 标签说明 | 备注 | 源码路径 |
| :--- | ---: | :---- | :---- | :--- |
| count | 个 | 网卡名 | 网卡上对应arp表数量  | collector/proc\_arp.lua |

### networks

这是网卡流量统计信息，已做差值处理

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

这里统计所有包状态，详细可以通过 pkt_logs 获取

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

统计所有包状态。[参考连接](https://developer.aliyun.com/article/484451)

 指标名 | 单位 | 标签说明 | 备注 | 源码路径 |
| :--- | ---: | :---- | :---- | :--- |
| frag\_inuse | 个 |  | 使用的IP段数量 | collector/proc\_sockstat.lua |
| frag\_memory | 页 |  | IP段使用内存数量 | collector/proc\_sockstat.lua |
| udplite\_inuse | 个 |  | udplite 使用量 | collector/proc\_sockstat.lua |
| udp\_mem | 页 |  | udp socket 内存使用量，含收发缓冲区队列 | collector/proc\_sockstat.lua |
| udp\_inuse | 个 |  | udp 使用量 | collector/proc\_sockstat.lua |
| tcp\_mem | 页 |  | udp socket 内存使用量，含收发缓冲区队列 | collector/proc\_sockstat.lua |
| tcp\_alloc | 个 |  | TCP socket 申请总数 | collector/proc\_sockstat.lua |
| tcp\_tw | 个 |  | TCP time wait socket 总数 | collector/proc\_sockstat.lua |
| tcp\_orphan | 个 |  | TCP ophan socket 总数 | collector/proc\_sockstat.lua |
| tcp\_inuse | 个 |  | TCP 常规 socket 总数 | collector/proc\_sockstat.lua |
| raw\_inuse | 个 |  | raw socket 使用量 | collector/proc\_sockstat.lua |
| sockets\_used | 个 |  | 总socket 使用量 | collector/proc\_sockstat.lua |


### softnets

This parser parses the stats from network devices. These stats includes events per cpu\(in row\), number of packets processed i.e packet_process \(first column\), number of packet drops packet\_drops \(second column\), time squeeze eg net\_rx\_action performed time_squeeze\(third column\), cpu collision eg collision occur while obtaining device lock while transmitting cpu\_collision packets \(eighth column\), received_rps number of times cpu woken up received\_rps \(ninth column\), number of times reached flow limit count flow\_limit\_count \(tenth column\), backlog status \(eleventh column\), core id \(twelfth column\).

| 指标名 | 单位 | 标签说明 | 备注 | 源码路径 |
| :--- | ---: | :---- | :---- | :--- |
| packet\_process | 个 | cpu，对应CPU号 | 所在核收包个数 | collector/proc\_softnet\_stat.lua |
| packet\_drop | 个 | cpu，对应CPU号 | 所在核丢包个数 | collector/proc\_softnet\_stat.lua |
| cpu\_collision | 个 | cpu，对应CPU号 | 是为了发送包而获取锁的时候有冲突的次数. | collector/proc\_softnet\_stat.lua |
| received\_rps | 个 | cpu，对应CPU号 | 这个 CPU 被其他 CPU 唤醒去收包的次数. | collector/proc\_softnet\_stat.lua |
| time\_squeeze | 个 | cpu，对应CPU号 | 函数 net\_rx\_action 调用次数. | collector/proc\_softnet\_stat.lua |
| flow\_limit\_count | 个 | cpu，对应CPU号 | 达到 flow limit 的次数. | collector/proc\_softnet\_stat.lua |

### cgroups 表

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

| 指标名 | 单位 | 标签说明 | 备注 | 源码路径 |
| :---   | --- | :---- | :---- | :--- |
| cpu      | - | CPU ID       |  | collector/proc\_interrupts.lua |
| 中断名称 | 次 | 中断触发次数 |  | collector/proc\_interrupts.lua |

### mounts 表

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
统计监控进程的statm信息

| 指标名 | 单位 | 标签说明 | 备注 | 源码路径 |
| :---   | --- | :---- | :---- | :--- |
| size     | - | total program size |  | collector/proc\_statm.lua |
| resident | - | resident set size |  | collector/proc\_statm.lua |
| shared   | - | number of resident shared pages |  | collector/proc\_statm.lua |
| text     | - | text (code) |  | collector/proc\_statm.lua |
| lib      | - | library |  | collector/proc\_statm.lua |
| data     | - | data + stack |  | collector/proc\_statm.lua |
| dt       | - | dirty pages |  | collector/proc\_statm.lua |
