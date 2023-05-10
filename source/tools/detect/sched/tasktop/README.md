# 功能说明
tasktop实现对task的cpu利用率进行监控
# 使用说明
```
USAGE: tasktop [--help] [-t] [-p TID] [-d DELAY] [-i ITERATION] [-s SORT] [-f LOGFILE] [-l LIMIT]

  -d, --delay=DELAY          采样的周期 默认采样区间为3秒
  -f, --logfile=LOGFILE      日志文件 默认为/var/log/sysak/tasktop/tasktop.log 指定为/dev/stdout每次输出前会刷屏 用于实时监控
  -i, --iter=ITERATION       采样的输出次数 默认是无限次
  -p, --pid=TID              指定监控的TID 如果是子线程ID 需要-t开启线程模式
  -s, --sort=SORT            采样结果的排序方式 可选项为user（用户态）、sys（内核态）、cpu（用户态+内核态）默认为cpu
  -t, --thread               线程模式 监控线程级的CPU利用率 默认为进程级
  -l, --limit=LIMIT          每次输出只显示的top-LIMIT个task 默认为所有CPU利用率大于0的task
```

# 使用举例
下面的例子对线程级task的CPU利用率进行采样 采样结果输出到标准输出流 排序方式为总的CPU利用率 连续采样两次 每次采样时间5s
```
sudo out/sysak tasktop -t -f /dev/stdout -s cpu -i 2 -d 5
```
## 日志输出说明
输出说明如下：
```
2023-04-28 06:38:09
           COMMAND    PID   PPID    RUNTIME %UTIME %STIME   %CPU
          (stress) 127560 127556          8  80.67   0.00  80.67
          (stress) 127563 127556          8  80.00   0.00  80.00
          (stress) 127557 127556          8  79.67   0.00  79.67
          (stress) 127559 127556          8  79.33   0.00  79.33
          (stress) 127562 127556          8  78.00   0.00  78.00
          (stress) 127564 127556          8  77.33   0.00  77.33
          (stress) 127558 127556          8  77.00   0.00  77.00
          (stress) 127561 127556          8  75.00   0.00  75.00
       (AliYunDun)  89000      1    1220822   2.67   1.33   4.00
       (AliYunDun)  89995      1    1220813   2.00   2.00   4.00
        (telegraf)   9555      1   65492482   0.33   2.00   2.33
        (telegraf)   9553      1   65492484   0.00   1.33   1.33
        (telegraf)   9546      1   65492484   0.00   1.33   1.33
        (telegraf)   9547      1   65492484   0.33   1.00   1.33
         (tasktop) 127637 127635          6   0.33   1.00   1.33
            (node)  93078  93041      19585   0.33   0.67   1.00
        (uniagent) 122088      1     185555   0.67   0.33   1.00
        (uniagent)  63591      1   10554696   0.33   0.33   0.67
        (telegraf)   9587      1   65492412   0.00   0.67   0.67
           (walle)  63777      1   10554689   0.33   0.33   0.67
       (AliYunDun)  88986      1    1220822   0.33   0.33   0.67   
```
上面的日志记录了一次采样周期内的CPU利用率大于0的task 并按照总CPU利用率排序。

输出的参数信息说明如下
```
COMMAND    命令名
PID        进程ID或者线程ID
PPID       父进程ID
RUNTIME    task的运行时间
%UTIME     用户态CPU利用率 
%STIME     内核态CPU利用率
%CPU       总CPU利用率
```
上面的日志可以看到PID为127556的task 创建了8个子进程占用了大量的CPU资源
