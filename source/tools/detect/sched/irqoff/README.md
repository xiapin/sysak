# 功能说明
基于eBPF实现的内核长时关中断检测分析功能  
irqoff使用eBPF结合perf采样的方式，根据内核支持hw perf事件的情况而采用不同的关中断检测机制。如果关中断时间过长、超过阈值，irqoff就将当前现场相关信息记录下来
# 使用说明
```
sysak irqoff  [--help] [-t THRESH(ms)] [-f LOGFILE] [duration(s)]
参数说明：
   -t  门限：   当关中断超过门限值就记录，单位ms；  ｜可选，默认10ms
   -f log文件： 将log记录到指定文件。               ｜可选，默认记录在/var/log/irqoff/irqoff.log
   durations：  设置该程序运行多长时间，单位秒；    ｜ 可选，默认永远运行
```
# 使用举例
## 运行说明
下面的例子使用irqoff采样30秒，当关中断超过门限10ms久记录到a.log文件
```
$sudo sysak irqoff  -f a.log -t 10 30   
```
## 日志输出说明
上面结果a.log输出说明如下：
```
$cat a.log  #输出如下（时间单位：毫秒）
时间戳             发生CP      任务名字         线程ID       中断延时     
  ｜                   \          ｜              ｜            ｜               
TIME(irqoff)           CPU       COMM            TID          LAT(ms)   
2022-05-26_17:50:58     3        kworker/3:0     379531       11 
<0xffffffffc04e2072> owner_func
<0xffffffff890b1c5b> process_one_work
<0xffffffff890b1eb9> worker_thread
<0xffffffff890b7818> kthread
<0xffffffff89a001ff> ret_from_fork
```
## 日志输出说明
该工具记录了系统长时间关中断的现场信息
-    CPU      系统长时间关中断的现场CPU
-    COMM  系统长时间关中断的现场任务名字
-    TID         系统长时间关中断的线程ID
-    LAT：    系统长时间关中断的时长
-    堆栈：发生系统长时间关中断时的现场运行堆栈
从上面的日志可以看出在17:50:58时刻任务kworker/3:0在CPU3经历了11ms关中断，造成这次关中断的堆栈在owner_func函数。
