# 功能说明
用于分析统计应用oncpu时间、offcpu原因及时间的工具，主要针对于java应用，以及在RT过程中进程号不发生改变的应用。
# 使用说明
```
USAGE: sysak rtdelay [--help] [-p PID]  [-d DURATION]
EXAMPLES:
    rtdelay             # trace RT time until Ctrl-C
    rtdelay -p 185      # only trace threads for PID 185
    rtdelay -d 10       # trace for 10 seconds only

  -d, --duration=DURATION    Total duration of trace in seconds
  -p, --pid=PID              Trace this PID only
  -?, --help                 Give this help list
      --usage                Give a short usage message
  -V, --version              Print program version
```
# 使用举例
## 运行说明
下面的例子使用java应用进行请求处理，并在请求处理过程中java应用发送请求获取服务器数据，服务器返回数据，java应用再进行请求返回。
```
$sudo sysak rtdelay -d 10 -p 91279
```
## 日志输出说明
上面结果输出说明如下：
```
 java应用收到请求时间戳  oncpu时间   runqueue时间 futex时间 lock时间  存储时间  等待网络时间  等待服务器返回时间  其他未分类时间
  ｜                   \             ｜        ｜           ｜      /      /             /           /
read_ts:104881254092291, on:1162, runqueue:7, futex:20, lock:0, io:0, net:0, server:3000329, other:0
```