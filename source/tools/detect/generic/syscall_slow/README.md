# 功能说明
基于ebpf开发的分析指定进程运行过程中系统调用慢的情况

# 使用说明
```
sysak syscall_slow [--help] [-t THRESH(ms)] [-n sys_NR] <[-c COMM]|[-p tid]> [-f LOGFILE][duration(s)]
   -t  门限：       系统调用超过该门限时就记录，单位ms; 可选，默认10ms；
   -n 系统调用号：  监控该系统调用(syscall号参考/usr/include/asm/unistd_64.h);可选，默认所有系统调用
   -c name/-p tid： 只检测指定任务名字/线程ID; 必选其中之一
   -f log文件：     将log记录到指定文件; 可选，默认记录在/var/log/sysak/syscall_slow/syscall_slow.log
   durations：      设置该程序运行多长时间，单位秒; 可选，默认永远运行
```

# 使用举例
## 运行说明
下面的例子使用syscall_slow监控名字为"cat"的任务的read系统调用，当系统调用超过门限10ms久记录到a.log文件
```
$sudo sysak syscall_slow -n 0 -c cat -f a.log 
```

## 日志输出说明
上面结果a.log输出说明如下(时间单位：毫秒；  切换单位：次数)：
``` 
               总延时   实际运行  被抢占时间  睡眠时间   系统态时间   自愿切换  被动切换  系统调用  任务名/id
                   \        \         ｜         ｜         ｜          ｜        /          /         |
TIME(syscall)        DELAY   REAL    WAIT       SLEEP       SYS        vcsw    ivcsw       syscall  pid(comm) 
2022-05-26_11:16:01  2484     0       0         2484        0          1       0           read     34562(cat)
<0xffffffff868814a3> __sched_text_start
<0xffffffff868814a3> __sched_text_start
<0xffffffff868816e3> schedule
<0xffffffff860fb8b5> do_syslog
<0xffffffff86321fce> kmsg_read
<0xffffffff8631386c> proc_reg_read
<0xffffffff8628ca79> vfs_read
<0xffffffff8628ceaa> ksys_read
<0xffffffff86003d4b> do_syscall_64
<0xffffffff86a00088> entry_SYSCALL_64_after_hwframe
```

## 日志输出说明
该工具记录了特定任务系统调用时间过长的现场信息
-    DELAY是该次系统调用总延时；
-    REAL  是任务在该次系统调用中实际的执行时间(包括用户态和内核态)；
-    WAIT 是任务在本次系统调用过程中被抢占的时间；
-    SLEEP：任务在本次系统调用睡眠的时间
-    SYS：任务在本次系统调用运行在内核态的时间
-    vcsw：主动调度出去的次数
-    ivcsw：被动调度出去的次数
-    syscall：系统调用号。
-    堆栈：本次系统调用任务最近一次切换出去的调用堆栈

