# 功能说明
iowait高时，统计iowait由哪些进程贡献
当系统中出现iowait高时，可使用此工具统计iowait是由哪些进程产生

# sysak打包
在编译sysak的之前，需要在执行configure配置的时候加上--enable-target-iowaitstat才能打包进sysak

# 使用
## 参数说明
```
sudo ./iowaitstat.py -h
usage: iowaitstat.py [-h] [-p PID] [-T TIMEOUT] [-t TOP] [-c CYCLE] [-j]
                     [-w IOWAIT_THRESH]

Report iowait for tasks.

optional arguments:
  -h, --help            show this help message and exit
  -p PID, --pid PID     Specify the process id.
  -T TIMEOUT, --Timeout TIMEOUT
                        Specify the timeout for program exit(secs).
  -t TOP, --top TOP     Report the TopN with the largest iowait.
  -c CYCLE, --cycle CYCLE
                        Specify refresh cycle(secs).
  -j, --json            Specify the json-format output.
  -w IOWAIT_THRESH, --iowait_thresh IOWAIT_THRESH
                        Specify the iowait-thresh to report.

e.g.
  ./iowaitstat.py
			Report iowait for tasks
  ./iowaitstat.py -c 1
			Report iowait for tasks per secs
  ./iowaitstat.py -p [PID] -c 1
			Report iowait for task with [PID] per 1secs
```
## 示例
```
sudo ./iowaitstat.py -c 1 #每秒统计每进程iowait贡献情况

2022/09/15 16:29:10 -> global iowait%: 0.0
comm                            tgid    pid     waitio(ms)      iowait(%)   reasons

2022/09/15 16:29:11 -> global iowait%: 6.8
comm                            tgid    pid     waitio(ms)      iowait(%)   reasons
dd                              10785   10785   722.276         6.8         Unkown[stacktrace:(iomap_dio_rw+0x390/0x410 -> io_schedule)]

2022/09/15 16:29:12 -> global iowait%: 9.72
comm                            tgid    pid     waitio(ms)      iowait(%)   reasons
dd                              10785   10785   871.048         9.72        Unkown[stacktrace:(iomap_dio_rw+0x390/0x410 -> io_schedule)]

2022/09/15 16:29:14 -> global iowait%: 8.63
comm                            tgid    pid     waitio(ms)      iowait(%)   reasons
dd                              10785   10785   871.549         8.63        Unkown[stacktrace:(iomap_dio_rw+0x390/0x410 -> io_schedule)]
awk                             -       13079   0.003           0.0         Unkown[stacktrace:(__lock_page_or_retry+0x1e7/0x4e0 -> io_schedule)]

2022/09/15 16:29:15 -> global iowait%: 5.71
comm                            tgid    pid     waitio(ms)      iowait(%)   reasons
kworker/u8:3                    51242   51242   804.93          3.74        Device queue full
java                            57284   57300   143.236         0.67        Device queue full
argusagent                      110584  110644  133.787         0.62        Device queue full
kworker/u8:4                    35568   35568   63.847          0.3         Device queue full
...
```
显示结果按照waitio(ms)作降序排列，如输出结果较多想只看某进程情况下，可以使用-p PID只查看指定进程，其中关键字段含义如下：
waitio(ms): 进程在1秒中累计等待io的时长
iowait(%) : 进程贡献的iowait百分比
reasons   : 引起等待io的原因

