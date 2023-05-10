# 功能说明
基于eBPF实现的监控指定任务调度延迟大(运行队列长)的工具
runqslower监控的是已经就绪(处于运行状态)的任务，但是由于各种原因(如就绪队列上排队任务太多等因素)导致被监控任务长时间无法得到CPU资源运行的情况。
# 使用说明
```
sysak  runqslower [-s SPAN] [-t TID]  [-f ./runqslow.log] [-P]  [threshold]
  threshold 门限：触发任务被抢占记录的门限值，单位ms; 可选，默认50ms
  -f log文件：    将log记录到指定文件; 可选，默认在/var/log/sysak/runqslow/runqslow.log
  -s durations：设置该程序运行多长时间，单位秒; 可选，默认永远运行
  -t  tid:       过滤选项，指定被监控的现场ID; 可选，默认监控所有的线程
  -P   : 日志中记录上次抢占的任务信息; 可选。
```
# 使用举例
## 运行说明
下面的例子使用runqslower对线程1001按照门限20ms采样30秒，采样的结果存放在当前目录a.log文件中
```
$sudo sysak runqslower  -f a.log -s 30  -t 1001  20
```
## 日志输出说明
上面结果a.log输出说明如下：
```
  时间戳          发生CPU        任务名字      线程ID         抢占延时    
                      \             ｜           ｜             ｜               
TIME(runslw)           CPU         COMM         TID            LAT(ms)   
2022-05-29_11:50:58    0           test         535832         20        
```
上面的日志记录了任务1001被长时间抢占的现场信息。
-   CPU      任务长时间被抢占的现场CPU；
-   COMM  任务长时间被抢占的现场任务名字；
-   TID         任务长时间被抢占的现场线程ID；
-   LAT：    任务长时间被抢占的延时时间。
从上面的日志可以看出在11:50:58时刻任务test被抢占超过20ms左右。
