# 监控指标采集 by lua
本文将开始讲解如何基于lua 开发proc 数据采集。

## 纯pystring 处理方法
预备知识，lua 

* [pystring](https://gitee.com/chuyansz/sysak/blob/opensource_branch/source/tools/monitor/unity/beaver/guide/pystring.md) 库，处理字符串
* [面向对象设计](https://gitee.com/chuyansz/sysak/blob/opensource_branch/source/tools/monitor/unity/beaver/guide/oop.md)

以提取 /proc/net/sockstat 数据为例，原始的信息如下：

```
#cat /proc/net/sockstat
sockets: used 83
TCP: inuse 6 orphan 0 tw 0 alloc 33 mem 2
UDP: inuse 6 mem 12
UDPLITE: inuse 0
RAW: inuse 0
FRAG: inuse 0 memory 0
```

### 数据处理策略
sockstat 接口导出的数据非常有规律，基本上是

```
[大标题]: [小标题] [值] ……
[大标题]: [小标题] [值] ……
```

这种方法进行组合，可以针对以上方式进行处理。

### 数据格式

监控使用 [protobuf](https://www.jianshu.com/p/a24c88c0526a) 来序列化和存取数据，标准数据.proto 文件描述如下：

```
	message labels {
                required string name   = 1;
                required string index  = 2;
                }
            message values {
                required string name   = 1;
                required double value  = 2;
                }
            message logs {
                required string name   = 1;
                required string log    = 2;
                }
            message  dataLine{
                required string line = 1;
                repeated labels  ls  = 2;
                repeated values  vs  = 3;
                repeated logs   log  = 4;
                }
            message  dataLines{
                repeated dataLine  lines = 1;
                }
           }
```

想了解监控 对 protobuf的处理，可以参考 [这个通用库](https://gitee.com/chuyansz/sysak/blob/opensource_branch/source/tools/monitor/unity/common/protoData.lua) 

### vproc 虚基础类
vproc 是所有 proc 接口数据采集的基础类，提供了通用的数据封装函数。根据前面的proto 文件描述，存储数据实质就是一堆数据表行组成的，在[vproc](https://gitee.com/chuyansz/sysak/blob/opensource_branch/source/tools/monitor/unity/collector/vproc.lua) 声明如下：

```
function CvProc:_packProto(head, labels, vs, log)
    return {line = head, ls = labels, vs = vs, log = log}
end
```

添加数据行：

```
function CvProc:appendLine(line)
    table.insert(self._lines["lines"], line)
end
```

将生成好的数据往外部table 中推送并清空本地数据：

```
function CvProc:push(lines)
    for _, v in ipairs(self._lines["lines"]) do
        table.insert(lines["lines"], v)
    end
    self._lines = nil
    return lines
end
```

### 整体代码实现
了解了vproc 类后，就可以从vproc 实现一个 /proc/net/sockstat 数据采集接口。代码 实现和注释如下：

```
require("class")  -- 面向对象 class 声明
local pystring = require("pystring")
local CvProc = require("vproc")

local CprocSockStat = class("procsockstat", CvProc)  -- 从vproc 继承

function CprocSockStat:_init_(proto, pffi, pFile)   -- 调用构造函数
    CvProc._init_(self, proto, pffi, pFile or "/proc/net/sockstat")
end

function CprocSockStat:proc(elapsed, lines)   -- 在主循环中会周期性调用proc 函数进行收集数据
    CvProc.proc(self)   -- 新建本地表
    local vs = {}    -- 用于暂存有效数据
    for line in io.lines(self.pFile) do    -- 读取文件内容
        local cells = pystring:split(line, ":", 1)   -- 按: 分割标题和内容
        if #cells > 1 then   -- 防止 空行产生无效数据
            local head, body = cells[1], cells[2]
            head = string.lower(head)  -- 标题统一小写
            body = pystring:lstrip(body, " ")   -- 去除开头的空格
            local bodies = pystring:split(body, " ")   -- 按空格分割内容
            local len = #bodies / 2
            for i = 1, len do
                local title = string.format("%s_%s", head, bodies[2 * i - 1])    -- 组合数值标题
                local v = {
                    name=title,  
                    value=tonumber(bodies[2 * i])
                }
                table.insert(vs, v)  -- 添加到暂存表中
            end
        end
    end
    self:appendLine(self:_packProto("sock_stat", nil, vs))  -- 保存到本地表中
    return self:push(lines)   --推送到全局表，并发送出去
end

return CprocSockStat   -- 这一行不能少
```

### 注册到主循环中
[loop.lua](https://gitee.com/chuyansz/sysak/blob/opensource_branch/source/tools/monitor/unity/collector/loop.lua) 是周期性采样所有数据的循环实现。首先将文件引入：

```
local CprocSockStat = require("proc_sockstat")
```

然后添加到collector 表中

```
CprocSockStat.new(self._proto, procffi),
```

此时数据已经保存在本地

### 导出到export

要将采集到的指标采集到export，只需要在 [plugin.yaml](https://gitee.com/chuyansz/sysak/blob/opensource_branch/source/tools/monitor/unity/collector/plugin.yaml) 中添加以下行做配置即可：

```
  - title: sysak_sock_stat
    from: sock_stat   # 代码中声明的表行
    head: value
    help: "sock stat counters from /proc/net/sockstat"
    type: "gauge"
```

### 数据呈现
用浏览器打开本地8400端口，到指标链接中，就可以提取到以下新增数据

```
# HELP sysak_sock_stat sock stat counters.
# TYPE sysak_sock_stat gauge
sysak_sock_stat{value="frag_inuse",instance="12345abdc"} 0.0
sysak_sock_stat{value="udplite_inuse",instance="12345abdc"} 0.0
sysak_sock_stat{value="udp_mem",instance="12345abdc"} 8.0
sysak_sock_stat{value="tcp_mem",instance="12345abdc"} 1.0
sysak_sock_stat{value="tcp_alloc",instance="12345abdc"} 32.0
sysak_sock_stat{value="frag_memory",instance="12345abdc"} 0.0
sysak_sock_stat{value="sockets_used",instance="12345abdc"} 80.0
sysak_sock_stat{value="raw_inuse",instance="12345abdc"} 0.0
sysak_sock_stat{value="tcp_tw",instance="12345abdc"} 0.0
sysak_sock_stat{value="tcp_orphan",instance="12345abdc"} 0.0
sysak_sock_stat{value="tcp_inuse",instance="12345abdc"} 5.0
```

[返回目录](/guide)