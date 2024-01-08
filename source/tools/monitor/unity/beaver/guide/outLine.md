# 外部数据写入支持
unity-mon可以作为一个独立的TSDB 数据库进行使用，支持[行协议](https://jasper-zhang1.gitbooks.io/influxdb/content/Write_protocols/line_protocol.html)写入数据，并按需完成对外数据吐出，如exporter等接口。

## 行协议格式说明
行协议使用换行符\n分隔每一行，每一行表示一个数据点，可以类比为关系型数据库中的一行。行协议是对空格敏感的。

```
<measurement>[,<tag_key>=<tag_value>[,<tag_key>=<tag_value>]] <field_key>=<field_value>[,<field_key>=<field_value>]
```
从语法中可以看出，行协议分为如下四个内容：measurement、tag set、field set
* measurement是必需的，可以类比为关系型数据库的表名。measurement类型是字符串。如一个描述气温的时序型数据库，measurement为"气温"。
* tag set不是必需的，用于描述一些不随时间变化的信息。tag_key和tag_value的类型均为字符串，如描述具体某地气温时，"城市"="深圳"。
* field set是必需的，一个数据点中必须有至少一个field。field用于描述随时间变化的信息。field_key的类型是字符串，field_value只能是浮点型或字符串。field_value为浮点型时表示数值，field_value为字符串时表示日志。

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