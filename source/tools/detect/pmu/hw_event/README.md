# 功能说明
基于c语言开发的轻量级硬件PMU事件检测工具
该功能主要是检测指定容器的cache miss比率和CPI;
一般情况下CPI大于1说明指令stall较高，可能受到访存影响

# 使用说明
```
Usage: sysak hw_event [-c container] [-s TIME]
  Options:
  -c container    container(docker) name or id, default all container 
  -s TIME         specify how long to run, default 5s 
eg. sysak hw_event -c pause -s 10
上例是检查容器pause 10秒内的cache miss比率、CPI等指标
```
