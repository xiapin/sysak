# 功能说明

    通过批量发起ping 探测，用于分析ping 抖动边沿位置
	
## 参数说明

    pingEdge [dist IP] [pid of container process] [minutes], minute is optional ,default is 5.
    例如，本地容器pid 为3551 进程 访问 192.168.0.131 存在抖动，需要探测3分钟的网络质量，可以按照以下命令执行探测：

```
ping 192.168.0.131 3551 3
```

     将会报告出容器访问目标IP，容器访问容器网关、host 访问目标IP，host 访问网关的网络质量，用于定界网络抖动边界。