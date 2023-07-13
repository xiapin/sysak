# 外部数据写入支持
unity-mon可以作为一个独立的TSDB 数据库进行使用，支持[行协议](https://jasper-zhang1.gitbooks.io/influxdb/content/Write_protocols/line_protocol.html)写入数据，并按需完成对外数据吐出，如exporter等接口。

## 行协议格式支持情况
unity-mon 当前除了不支持时间戳，支持行协议其它所有的数据类型，包含数值和日志。写行数据时，有以下注意事项：

* 指标写入周期需要与大循环刷新周期保持一致，参考 yaml/config/freq 参数配置；

* 不要将同一表名和同一索引，但数值不同的数据放在同一批次写入操作中，会发生时序数据覆盖，如；

```
talbe_a,index=table_a value1=1,value2=2
talbe_a,index=table_a value1=3,value2=4
```

* 同一张表 写同一批数据应该要保持一致，如

```
talbe_a,index=table_a value1=1,value2=2
talbe_a,index=table_b value1=3,value2=4
```

* 不要出现同一张表，但是写入的索引和数值不的情况，如：

```
talbe_a,index=table_a value1=1
talbe_a,index=table_b value2=3
```

这样操作会导致数据库中的数值需要用空值来表示，为后面的数据解析带来极大地不便，可以将上面两行数据合并成一行数据，或者拆分成两个table 来分开写入

## 外部数据写入示例
unity-mon 同时支持管道和http post 两种方式进行写入，两者差别如下：

| 模式 | pipe | http |
| --- | --- | --- |
| 适用范围 | 内部 | 内部 + 外部 |
| 写入效率 | 高 | 低 |
| 最大单次写入数据长度 | 64K | 2M |

使用者可以结合自己的实际情况进行推送

### pipe api

```
import os
import socket

PIPE_PATH = "/var/sysom/outline"  # 这里可以参考yaml 中的配置
MAX_BUFF = 64 * 1024


class CnfPut(object):
    def __init__(self, path=PIPE_PATH):
        self._sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
        self._path = path
        if not os.path.exists(self._path):
            raise ValueError("pipe path is not exist. please check Netinfo is running.")

    def puts(self, s):
        if len(s) > MAX_BUFF:
            raise ValueError("message len %d, is too long ,should less than%d" % (len(s), MAX_BUFF))
        return self._sock.sendto(s, self._path)


if __name__ == "__main__":
    nf = CnfPut()
    nf.puts('runtime,mode=java log="hello runtime."')
```

### http post

```
import requests

url = "http://127.0.0.1:8400/api/line"
line = "lineTable,index=abc value=2"
res = requests.post(url, data=line)
print(res)   # 成功返回 200
```

## 数据反查入口
写入到unity-mon 数据后，一般会缓存7天左右，有一个[查询入口](http://127.0.01:8400/query/base)可以查询到最近写入数据，确定数据是否已经写入本地监控。