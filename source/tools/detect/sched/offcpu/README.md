# 功能说明
采集系统中任务off cpu情况  
offcpu是一款基于ebpf的用于采集系统中任务从on-cpu状态到off-cpu状态的统计分析工具。 
# 使用说明
```
USAGE: offcputime [--help] [-p PID | -u | -k] [-f] [-m MIN-BLOCK-TIME] [-M
MAX-BLOCK-TIME] [--state] [--perf-max-stack-depth] [--stack-storage-size]
[duration]
  -k, --kernel-threads-only  Kernel threads only (no user threads)
  -m, --min-block-time=MIN-BLOCK-TIME
                             the amount of time in microseconds over which we
                             store traces (default 1)
  -M, --max-block-time=MAX-BLOCK-TIME
                             the amount of time in microseconds under which we
                             store traces (default U64_MAX)
  -p, --pid=PID              Trace this PID only
      --perf-max-stack-depth=PERF-MAX-STACK-DEPTH
                             the limit for both kernel and user stack traces
                             (default 127)
      --stack-storage-size=STACK-STORAGE-SIZE
                             the number of unique stack traces that can be
                             stored and displayed (default 1024)
      --state=STATE          filter on this thread state bitmask (eg, 2 ==
                             TASK_UNINTERRUPTIBLE) see include/linux/sched.h
  -t, --tid=TID              Trace this TID only
  -f, --folded          output folded format
  -u, --user-threads-only    User threads only (no kernel threads)
  -v, --verbose              Verbose debug output
  -?, --help                 Give this help list
      --usage                Give a short usage message
  -V, --version              Print program version
```
# 使用举例
## 运行说明
下面的例子使用offcputime采样30秒，采样的结果存放在当前目录offcputime.fl文件中
```
$sudo sysak offcputime -f -u 30 > offcputime.fl
git clone https://github.com/brendangregg/FlameGraph.git
./flamegraph.pl --color=io --title="Off-CPU Time Flame Graph" --countname=us offcputime.fl > offcputime.svg
 
```
