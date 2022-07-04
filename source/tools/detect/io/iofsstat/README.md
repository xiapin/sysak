# 功能说明
实现从进程和文件级别统计IO信息
传统的IO统计工具在如下场景下会略有不足：
1.在磁盘io被打满的情况下，希望观察是哪个进程贡献了比较多的IO，传统的工具只能从整个磁盘角度去统计io信息，如统计整盘的iops、bps，但不能统计单个进程所贡献的iops、bps
2.系统上统计到某个进程贡献了大量的IO，希望观察到这些IO最终是被哪个磁盘给消费，或者这些IO是在访问哪个文件，如果这个进程是来自某个容器，希望依然可以获取访问的文件以及此进程所在的容器

# sysak打包
在编译sysak的之前，需要在执行configure配置的时候加上--enable-target-iofsstat才能打包进sysak

# 使用
## 参数说明
```
$sudo ./iofsstat.py -h
usage: iofsstat.py [-h] [-T TIMEOUT] [-t TOP] [-u UTIL_THRESH] [-b BW_THRESH]
                   [-i IOPS_THRESH] [-c CYCLE] [-d DEVICE] [-p PID] [-j] [-f]

Report IO statistic for partitions.

optional arguments:
  -h, --help            show this help message and exit
  -T TIMEOUT, --Timeout TIMEOUT
                        Specify the timeout for program exit(secs).
  -t TOP, --top TOP     Report the TopN with the largest IO resources.
  -u UTIL_THRESH, --util_thresh UTIL_THRESH
                        Specify the util-thresh to report.(这个参数可以过滤util低于此阈值的信息)
  -b BW_THRESH, --bw_thresh BW_THRESH
                        Specify the BW-thresh to report.(这个参数可以过滤bw低于此阈值的信息)
  -i IOPS_THRESH, --iops_thresh IOPS_THRESH
                        Specify the IOPS-thresh to report.(这个参数可以过滤iops低于此阈值的信息)
  -c CYCLE, --cycle CYCLE
                        Specify refresh cycle(secs).
  -d DEVICE, --device DEVICE
                        Specify the disk name.
  -p PID, --pid PID     Specify the process id.
  -j, --json            Specify the json-format output.
  -f, --fs              Report filesystem statistic for partitions.

e.g.
  ./iofsstat.py -d vda -c 1
			Report IO statistic for vda per 1secs
  ./iofsstat.py -d vda1 --fs -c 1
			Report fs IO statistic for vda1 per 1secs
```
## block-layer io统计
```
./iofsstat.py #统计一次全局io情况

2022/06/15 11:39:58
device-stat:        r_iops  w_iops  r_bps       w_bps       wait    r_wait  w_wait  util%
vda                 0.0     1.0     0           36.0KB/s    0.0     0       0.0     0.0
vdb                 0.0     0.0     0           0           0       0       0       0.0
vda1                0.0     0.0     0           0           0       0       0       0.0
vda2                0.0     1.0     0           36.0KB/s    1.0     0       1.0     0.0

comm                pid     iops_rd     bps_rd          iops_wr     bps_wr      device
kworker/u8:1        43258   0           0               1           4.0KB/s     vda

./iofsstat.py -d vda #统计一次vda磁盘io情况
2022/06/15 11:41:37
device-stat:        r_iops  w_iops  r_bps       w_bps       wait    r_wait  w_wait  util%
vda                 0.0     9.0     0           152.0KB/s   0.67    0       0.67    0.2

comm                pid     iops_rd     bps_rd          iops_wr     bps_wr      device
kworker/u8:1        43258   0           0               5           20.0KB/s    vda
jbd2/vda2-8         605     0           0               2           8.0KB/s     vda
hostinfo            68983   0           0               2           8.0KB/s     vda

$./iofsstat.py -d vda -c 1 #间隔1秒统计一次vda磁盘io情况
2022/06/15 11:42:22
device-stat:        r_iops  w_iops  r_bps       w_bps       wait    r_wait  w_wait  util%
vda                 0.0     1.0     0           8.0KB/s     0.0     0       0.0     0.0

comm                pid     iops_rd     bps_rd          iops_wr     bps_wr      device
kworker/u8:1        43258   0           0               1           4.0KB/s     vda

2022/06/15 11:42:23
device-stat:        r_iops  w_iops  r_bps       w_bps       wait    r_wait  w_wait  util%
vda                 0.0     13.0    0           156.0KB/s   1.38    0       1.38    0.2

comm                pid     iops_rd     bps_rd          iops_wr     bps_wr      device
kworker/u8:1        43258   0           0               11          68.0KB/s    vda
jbd2/vda2-8         605     0           0               2           8.0KB/s     vda
...
```
显示结果按照iops_rd与iops_wr的和作降序排列，如输出结果较多想只看某进程情况下，可以使用-p PID只查看指定进程，其中关键字段含义如下：
iops_rd: 进程贡献的读iops
bps_rd : 进程贡献的读bps
iops_wr: 进程贡献的写iops
bps_wr : 进程贡献的写bps
device : 消费IO的具体设备

## fs-layer io统计
```
./iofsstat.py --fs -c 1 -t 5 #间隔1秒统计一次全局文件io情况，只查看前top5

2022/06/15 11:45:13
device-stat:        r_iops  w_iops  r_bps       w_bps       wait    r_wait  w_wait  util%
vda                 0.0     930.0   0           413.6MB/s   255.23  0       255.23  100.0
vdb                 0.0     0.0     0           0           0       0       0       0.0
vda1                0.0     0.0     0           0           0       0       0       0.0
vda2                0.0     930.0   0           413.6MB/s   255.23  0       255.23  100.0

comm                tgid    pid     cnt_rd  bw_rd       cnt_wr  bw_wr       inode       device      filepath
dd                  73372   73372   0       0           3436    429.5MB/s   2765753     vda2        /home/test/test12
testpasswd          117127  117151  115     460.0KB/s   0       0           264737      vda2        /etc/passwd
su                  73635   73635   93      372.0KB/s   0       0           264734      vda2        /etc/group
su                  73635   73635   20      80.0KB/s    0       0           1050440     vda2        /.../usrshare/locale/locale.alias
su                  73635   73635   8       32.0KB/s    0       0           1312377     vda2        /.../sharez/si/Shai

^C2022/06/15 11:45:13
device-stat:        r_iops  w_iops  r_bps       w_bps       wait    r_wait  w_wait  util%
vda                 0.0     513.0   0           228.6MB/s   241.17  0       241.17  47.0
vdb                 0.0     0.0     0           0           0       0       0       0.0
vda1                0.0     0.0     0           0           0       0       0       0.0
vda2                0.0     513.0   0           228.6MB/s   241.17  0       241.17  53.4

comm                tgid    pid     cnt_rd  bw_rd       cnt_wr  bw_wr       inode       device      filepath
dd                  73372   73372   0       0           1865    233.1MB/s   2765753     vda2        /home/test/test12
test1               51465   51465   2       2.0KB/s     0       0           1317442     vda2        /var/log/secure
test2               117127  117149  0       0           3       734.0B/s    2105572     vda2        /home/testag/log/test.log
test3               51465   51651   5       672.0B/s    0       0           1050789     vda2        /usr/bin/bash
test4               51465   51467   0       0           1       181.0B/s    1183400     vda2        /.../test4data/data/data.3
...
```
显示结果按照bw_rd与bw_wr的和作降序排列，如输出结果较多想只看某进程情况下，可以使用-p PID只查看指定进程，其中关键字段含义如下：
cnt_rd: 读文件次数
bw_rd : 读文件"带宽"
cnt_wr: 写文件次数
bw_wr : 写文件"带宽"
inode : 文件inode编号
filepath: 文件路径, 当在一次采集周期内由于进程访问文件很快结束情况下，获取不到文件名则为"-"
如进程来自某个容器，在文件名后缀会显示[containterId:xxxxxx]（note: iofsstat在宿主机上运行情况下，如果是在容器中运行，展示的就是容器中的文件路径，不会有这个后缀）
如文件路径中出现‘...’表示是一个不完全路径，‘...’省略了部分目录关系，如上面案例的/.../test4data/data/data.3
表示test4进程在操作test4data/data/data.3文件，test4data与‘/’之间应还存在几层目录
