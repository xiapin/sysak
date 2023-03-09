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

### softnets

This parser parses the stats from network devices. These stats includes events per cpu\(in row\), number of packets processed i.e packet_process \(first column\), number of packet drops packet\_drops \(second column\), time squeeze eg net\_rx\_action performed time_squeeze\(third column\), cpu collision eg collision occur while obtaining device lock while transmitting cpu\_collision packets \(eighth column\), received_rps number of times cpu woken up received\_rps \(ninth column\), number of times reached flow limit count flow\_limit\_count \(tenth column\), backlog status \(eleventh column\), core id \(twelfth column\).

| 指标名 | 单位 | 标签说明 | 备注 | 源码路径 |
| :--- | ---: | :---- | :---- | :--- |
| packet\_process | 个 | cpu，对应CPU号 | 所在核收包个数 | collector/proc\_softnet\_stat.lua |
| packet\_drop | 个 | cpu，对应CPU号 | 所在核丢包个数 | collector/proc\_softnet\_stat.lua |
| cpu\_collision | 个 | cpu，对应CPU号 | number of times reached flow limit count. | collector/proc\_softnet\_stat.lua |
| received\_rps | 个 | cpu，对应CPU号 | number of times cpu woken up received_rps. | collector/proc\_softnet\_stat.lua |
| time\_squeeze | 个 | cpu，对应CPU号 | net\_rx\_action. | collector/proc\_softnet\_stat.lua |
| flow\_limit\_count | 个 | cpu，对应CPU号 | number of times reached flow limit count. | collector/proc\_softnet\_stat.lua |
