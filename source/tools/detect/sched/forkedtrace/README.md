# 功能说明
监控并分析系统长时不调度的情况  
nosched是一款基于ebpf的用于监控cpu长时间运行在系统态，使得内核无法进入调度流程从而导致其他任务得不到调度的工具。 
# 使用说明
```
sysak nosched [--help] [-t THRESH(ms)]   [-f LOGFILE] [-s duration(s)]  
    -t  门限：    当内核超过门限时间不调度就记录，单位ms; 可选，默认10ms
    -f log文件：  将log记录到指定文件;可选，默认在/var/log/sysak/nosched/nosched.log
    -s durations：设置该程序运行多长时间，单位秒; 可选，默认永远运行
```
# 使用举例
## 运行说明
下面的例子使用nosched采样30秒，采样的结果存放在当前目录a.log文件中
```
$sudo sysak nosched  -f a.log -s 30  
```
## 日志输出说明
上面结果a.log输出说明如下：
```
  时间戳     发生CPU   任务名字    线程ID   内核延时   时间
    ｜           \        ｜         ｜        ｜       |          
TIME(nosched)    CPU     COMM       TID      LAT(ms)   DATE
1645141.062124   0       test       535832   10        2022-07-31 18:14:50
<0xffffffff86a01b0f> apic_timer_interrupt
<0xffffffff868744a7> clear_page_erms
<0xffffffff861e6add> prep_new_page
<0xffffffff861ead9f> get_page_from_freelist
<0xffffffff861ec47e> __alloc_pages_nodemask
<0xffffffff8624cee3> alloc_pages_vma
<0xffffffff8621f2a8> do_anonymous_page
<0xffffffff86224f75> __handle_mm_fault
<0xffffffff86225436> handle_mm_fault
<0xffffffff8606fc9a> __do_page_fault
<0xffffffff8606ff92> do_page_fault
<0xffffffff86a0119e> async_page_fault  
```
上面的日志记录了机器系统态长时间运行的现场信息。
-    CPU      发生内核态长时间运行不调度时的现场CPU；
-    COMM  发生内核态长时间运行不调度时的现场任务名字；
-    TID         发生内核态长时间运行不调度时的现场的线程ID；
-    LAT：    内核态长时间运行不调度的时长。
-    堆栈：发生内核态长时间运行不调度时的现场的运行堆栈
从上面的日志可以看出在17:50:58时刻任务test在内核运行了10ms左右，造成这个延时到原因是发生了缺页异常。



