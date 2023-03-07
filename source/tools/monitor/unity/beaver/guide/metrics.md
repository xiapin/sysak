# 指标说明

这里记录所有采集到的监控指标说明和来源，方便监控系统集成。

## 通用指标

-------------

### uptime 表 

| 指标名 | 单位 | 标签说明 | 备注 | 源码路径 |
| :--- | ---: | :---- | :---- | :--- |
| uptime | 秒 | 从系统启动到现在的时间 |  | collector/proc_uptime.lua |
| idletime | 秒 | 系统总空闲的时间 |  | collector/proc_uptime.lua |
| stamp | 秒 | 系统时间戳 | unix 时间  | collector/proc_uptime.lua |

### uname 表

每小时获取一次

| 指标名 | 单位 | 标签说明 | 备注 | 源码路径 |
| :--- | ---: | :---- | :---- | :--- |
| nodename | - | uname -r |  | collector/proc_uptime.lua |
| version | - | uname -r |  | collector/proc_uptime.lua |
| release | - | uname -r |  | collector/proc_uptime.lua |
| machine | - | uname -r |  | collector/proc_uptime.lua |
| sysname | - | uname -r |  | collector/proc_uptime.lua |

