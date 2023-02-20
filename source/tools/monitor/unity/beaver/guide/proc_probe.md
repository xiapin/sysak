# proc 和 libbpf probes 列表

为了避免proc 和 libbpf kprobe/kretprobe/trace\_event/perf event 等事件重复采集，将 proc 和 probes 统一记录在该文档内进行采集。

## proc 记录入口

| proc名 | 对应代码路径 |
| ----- | --------- |
| proc/stat | /collector/proc\_stat.lua |
| proc/meminfo | /collector/proc\_meminfo.lua |
| proc/vmstat | /collector/proc\_vmstat.lua |
| /proc/diskstats | /collector/proc\_diskstats.lua |
| /proc/net/dev | /collector/proc\_netdev.lua |
| /proc/schedstat | /collector/plugin/proc\_schedstat.c |
| /proc/loadavg | /collector/plugin/proc\_loadavg.c |

## libbpf probes

libbpf kprobe/kretprobe/trace\_event/perf event 等事件记录在这里

| probe名 | 对应代码路径 |
| ----- | --------- |
|  xxx | xxx |

[返回目录](/guide/guide.md)
