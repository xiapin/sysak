# 功能说明
基于ebpf开发的信号发送者跟踪工具

# 使用说明
```
sysak tracesig [--help] <-c COMM> [-f LOGFILE][duration(s)]
   -c name：        只检测指定任务名字
   -f log文件：     将log记录到指定文件
   s durations：    设置该程序运行多长时间，单位秒; 可选，默认永远运行
```

# 使用举例
## 运行说明
下面的例子使用tracesig监控名字为loop的任务收到信号的情况，并记录到a.log文件
```
$sudo sysak tracesig -c loop -f a.log
```

## 日志输出说明
上面结果a.log输出说明如下(时间单位：毫秒；  切换单位：次数)：
``` 
$cat a.log
kill event happend at   2022-12-27 16:44:56
killall[261280] kill -15 to sigtest1[261276]
murderer information:
 cwd:/root/tmp/
 cmdline=sh -c ./test.sh
 parent=sh[261275]

kill event happend at   2022-12-27 16:44:56
killall[261280] kill -15 to sigtest2[261277]
murderer information:
 cwd:/root/tmp/
 cmdline=sh -c ./test.sh
 parent=sh[261275]

kill event happend at   2022-12-27 16:44:56
killall[261280] kill -15 to sigtest3[261278]
murderer information:
 cwd:/root/tmp/
 cmdline=sh -c ./test.sh
 parent=sh[261275]
```

## 日志输出说明
-    时间: 发生kill的时间点
-    信号发送者与接受者
-    cwd: 信号发送者的cwd路径
-    cmdline: 信号发送者的cmdline
-    parent: 信号发送者的parent信息

