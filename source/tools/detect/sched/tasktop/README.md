# 功能说明
针对于负载问题的分析工具，相比loadtask对于细分场景进行了覆盖。

# 使用说明

## 基础用法

```bash
Usage: tasktop [OPTION...]
Load analyze & D stack catch.

USAGE:
load analyze: tasktop [--help] [-t] [-p TID] [-d DELAY] [-i ITERATION] [-s
SORT] [-f LOGFILE] [-l LIMIT] [-H] [-e D-LIMIT]
catch D task stack: tasktop [--mode blocked] [--threshold TIME] [--run TIME]

EXAMPLES:
1. Load analyze examples:
    tasktop            # run forever, display the cpu utilization.
    tasktop -t         # display all thread.
    tasktop -p 1100    # only display task with pid 1100.
    tasktop -d 5       # modify the sample interval.
    tasktop -i 3       # output 3 times then exit.
    tasktop -s user    # top tasks sorted by user time.
    tasktop -l 20      # limit the records number no more than 20.
    tasktop -e 10      # limit the d-stack no more than 10, default is 20.
    tasktop -H         # output time string, not timestamp.
    tasktop -f a.log   # log to a.log.
    tasktop -e 10      # most record 10 d-task stack.

2. blocked analyze examples:
    tasktop --mode blocked --threshold 1000 --run 120 # tasktop run (120s)
catch the task that blocked in D more than (1000 ms)


  -b, --threshold=TIME(ms)   dtask blocked threshold, default is 3000 ms
  -d, --delay=DELAY          Sample peroid, default is 3 seconds
  -e, --d-limit=D-LIMIT      Specify the D-LIMIT D tasks's stack to display
  -f, --logfile=LOGFILE      Logfile for result, default
                             /var/log/sysak/tasktop/tasktop.log
  -H, --human                Output human-readable time info.
  -i, --iter=ITERATION       Output times, default run forever
  -k, --kthread              blocked-analyze output kernel-thread D stack
                             information
  -l, --r-limit=LIMIT        Specify the top R-LIMIT tasks to display
  -m, --mode=MODE            MODE is load or blocked, default is load
  -p, --pid=TID              Specify thread TID
  -r, --run=TIME(s)          run time in secnonds
  -s, --sort=SORT            Sort the result, available options are user, sys
                             and cpu, default is cpu
  -t, --thread               Thread mode, default process
  -v, --verbose              ebpf program output verbose message
  -?, --help                 Give this help list
      --usage                Give a short usage message
  -V, --version              Print program version

Mandatory or optional arguments to long options are also mandatory or optional
for any corresponding short options.
```

## 典型使用场景

### 负载问题分析

详细的使用案例，请查看`tasktopSelftest`目录下的`test.md`。

### 抓取进程D状态超时栈

如果是负载高由D进程导致，可以抓取超时的D进程栈实现进一步地定位和分析。

#### 具体使用方法
```bash
sysak tasktop --mode blocked --threshold 1000 -f /dev/stdout --kthread --run 60

--mode: 抓取D进程超时栈
--threshold: 设定超时门限，单位毫秒。默认为3000ms。
-f: 设定日志输出位置
--kthread: 抓取内核线程超时栈，默认只抓取用户进程。
--run: 运行持续时间，单位秒，默认永久运行。
```

#### 输出数据格式
  输出数据包含两类事件，Timeout为超时事件，当task的D状态持续时间达到超时门限时会输出该事件以及对应的内核栈；Stop-D为task的D状态结束事件，此时会输出D进程具体的持续时长。`Start(ns)`为该次D状态发生时的时间戳，该值为系统自启动以来的运行总时长。可以通过`Start(ns)`和`pid`属性确认该次超时发生在哪个进程的哪一次D状态睡眠。同时通过`Start(ns)`可以分析不同进程进入D状态的事件顺序，以及分析同一进程多次进入D进程的事件顺序。

* Time: 日志时间戳
* Event: 事件类型，分为超时事件（Timeout）以及D状态结束事件（Stop-D）。
* Comm: 进程命令
* Pid: 进程ID
* Start(ns): 进入D状态时的时间戳，为系统启动以来运行总时长。
* Stack|Delya(ms): 事件不同时输出不同的信息，Timeout事件输出超时的内核栈，Stop-D事件输出D状态持续时长，单位为ms。

```bash
                Time    Event  Comm    Pid          Start(ns)  Stack|Delya(ms)
 2023-10-31 07:08:03  Timeout   cat  45722  13817098190487947  [<0>] open_proc+0x53/0x7e [mutex_block]
                                                               [<0>] proc_reg_open+0x72/0x130
                                                               [<0>] do_dentry_open+0x23a/0x3a0
                                                               [<0>] path_openat+0x768/0x13e0
                                                               [<0>] do_filp_open+0x99/0x110
                                                               [<0>] do_sys_open+0x12e/0x210
                                                               [<0>] do_syscall_64+0x55/0x1a0
                                                               [<0>] entry_SYSCALL_64_after_hwframe+0x44/0xa9
                                                               [<0>] 0xffffffffffffffff
 2023-10-31 07:08:06   Stop-D   cat  45722  13817098190487947  3052
 2023-10-31 07:08:08  Timeout   cat  45885  13817103246221434  [<0>] open_proc+0x53/0x7e [mutex_block]
                                                               [<0>] proc_reg_open+0x72/0x130
                                                               [<0>] do_dentry_open+0x23a/0x3a0
                                                               [<0>] path_openat+0x768/0x13e0
                                                               [<0>] do_filp_open+0x99/0x110
                                                               [<0>] do_sys_open+0x12e/0x210
                                                               [<0>] do_syscall_64+0x55/0x1a0
                                                               [<0>] entry_SYSCALL_64_after_hwframe+0x44/0xa9
                                                               [<0>] 0xffffffffffffffff
 2023-10-31 07:08:11   Stop-D   cat  45885  13817103246221434  3052
 2023-10-31 07:08:13  Timeout   cat  45996  13817108302333045  [<0>] open_proc+0x53/0x7e [mutex_block]
                                                               [<0>] proc_reg_open+0x72/0x130
                                                               [<0>] do_dentry_open+0x23a/0x3a0
                                                               [<0>] path_openat+0x768/0x13e0
                                                               [<0>] do_filp_open+0x99/0x110
                                                               [<0>] do_sys_open+0x12e/0x210
                                                               [<0>] do_syscall_64+0x55/0x1a0
                                                               [<0>] entry_SYSCALL_64_after_hwframe+0x44/0xa9
                                                               [<0>] 0xffffffffffffffff
 2023-10-31 07:08:16   Stop-D   cat  45996  13817108302333045  3052
```

