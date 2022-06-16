# numa_access
numa_access可根据指定进程、CPU采集指令条数、CPU cycle数、内存在l3 miss的情况下的跨numa访存情况。numa_access基于perf开发，当前numa_access仅支持intel x86处理器。

# 使用
## 参数说明
```
$./sysak numa_access -h
sysak numa_access: show numa access information
options: -h help information
         -p pid, specify the pid
         -c cpu, specify the cpu
         -i interval, the interval checking the numa access times
```

## 案例
```
./sysak numa_access -p 624499 -i 2
Counting numa access times... Hit Ctrl-C to end.
pid: 624499, cpu: all, interval: 2
IPC                 Instructions        Cycles              Local-Dram-Access   Remote-Dram-Access  RDA-Rate
0.47                8,362,719,284       17,696,739,977      14,740,627          24,203,774          0.62
```
其中，RDA-Rate为跨numa访存率：RDA-Rate=Remote-Dram-Access/(Local-Dram-Access + Remote-Dram-Access)
