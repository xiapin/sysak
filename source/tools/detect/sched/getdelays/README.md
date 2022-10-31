# getdelays 功能说明
检测IO/内存回收/内存交换/任务抢占/中断等事件引发的任务延时情况
getdelays是一个基于netlink和eBPF实现的任务延迟监测分析工具，工具取得指定周期内指定任务由于系统的IO、内存回收、内存交换、任务抢占、中断打断等因素的干扰造成的延时时间。
## 构建

### getdelays内核依赖
getdelays内核上依赖于CONFIG_TASKSTATS，多数发行版都支持该选项  

### getdelays编译依赖 
getdelays的编译要求clang10.1以上版本
```
make
```

## 运行
要运行getdelays，请保证如下位置之一有内核btf相关文件：
- /sys/kernel/btf/vmlinux
- /boot/vmlinux-<kernel_release>
- /lib/modules/<kernel_release>/vmlinux-<kernel_release>
- /lib/modules/<kernel_release>/build/vmlinux
- /usr/lib/modules/<kernel_release>/kernel/vmlinux
- /usr/lib/debug/boot/vmlinux-<kernel_release>
- /usr/lib/debug/boot/vmlinux-<kernel_release>.debug
- /usr/lib/debug/lib/modules/<kernel_release>/vmlinux

## 使用
### 命令行参数
```
USAGE: getdelays [--help] <-t TGID|-P PID> [-f ./res.log] [span times]

EXAMPLES:
    getdelays -p 123          # trace pid 123(for threads only)
    getdelays -t 123          # trace tgid 123
    getdelays -p 123 -f a.log # record result to a.log (default:/var/log/sysak/getdelays.log)
    getdelays -p 123 10       # monitor for 10 seconds

  -f, --logfile=LOGFILE      logfile for result
  -p, --pid=PID              Thread PID to trace
  -t, --tid=TGID             Process TGID to trace
  -v, --verbose              Verbose debug output
  -?, --help                 Give this help list
      --usage                Give a short usage message
  -V, --version              Print program version

```
### 使用示例
抓取进程8733中所有线程的延时信息，持续60秒，结果存放在当前目录a.log文件中  
```
sudo sysak getdelays -t 8733 -f a.log 60
```
### 结果说明
```
$cat a.log 
WHAT        count          delay total    delay average  (ms)
CPU         12698          10.453         0.001          
IO          0              0.000          0.000          
SWAP        0              0.000          0.000          
RECLAIM     0              0.000          0.000          
IRQ         372            1.877          0.005 
```
分析上面日志a.log读取的内容，进程8733中各个线程60秒内的延迟信息如下：  
- 被抢占12698次，抢占时间10.453ms，平均抢占时间0.001ms
- 发生IO事件0次
- 发生SWAP-IN事件0次
- 发生内存回收事件0次
- 发生中断372次，中断总时间1.877ms，平均每次中断的时间为0.005ms
