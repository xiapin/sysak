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

## FFI 处理方式
关于lua ffi 说明，可以先参考[lua扩展ffi](https://luajit.org/ext_ffi.html)，本质是lua 可以通过ffi 接口直接调用C库参数，无需经过中间栈上传参等操作。

ffi的注意点：

* ffi 数组下标是从0开始，和lua下标从1开始不一样；
* 可以直接引用ffi 中的数据结构，效率要比原生lua 高很多；
* ffi 是luajit 的功能，原生lua 并不支持；

### 为什么要使用ffi？
pystring 虽然可以高效处理字符串数据，但是相比c语言中的scanf 接口来说效率还是要低很多。因此按行读取proc 数据，可以采用 ffi 接口来显著提升数据处理效率

### ffi 数据结构和api 说明

proc 数据以变参为主，下面的结构体主要用于scanf 获取变参， 用于上层数据处理

```
#define VAR_INDEX_MAX 64

// 变参整数类型，用于收集纯整数类型的数据
typedef struct var_long {
    int no;    // 收集到参数数量
    long long value[VAR_INDEX_MAX];   //参数列表
}var_long_t;

// 变参字符串类型
typedef struct var_string {
    int no; // 收集到参数数量
    char s[VAR_INDEX_MAX][32];   //参数列表
}var_string_t;

// 变参 k vs 类型
typedef struct var_kvs {  
    int no;  // 收集到参数数量
    char s[32];   // 标题
    long long value[VAR_INDEX_MAX];   // 参数列表
}var_kvs_t;
```

导出的c api

```
int var_input_long(const char * line, struct var_long *p);
int var_input_string(const char * line, struct var_string *p);
int var_input_kvs(const char * line, struct var_kvs *p);
```

综合来说：

* var\_long\_t 适合纯整数数字输出的场景
* var\_string\_t 适合纯字符串输出的场景
* var\_kvs\_t 适合单字符串 + 多整形数字 组合的场景，如 /proc/stat的内容输出

其它重复组合场景可以先按照 var\_string\_t 来收集，然后对指定位置的数字字符串通过tonumber 进行转换。

### 实际应用例子
以[kvProc.lua](https://gitee.com/chuyansz/sysak/blob/opensource_branch/source/tools/monitor/unity/collector/kvProc.lua) 为例，它实现了一个通用kv组合的proc接口数据的数据高效的处理方法。如经常使用到的 /proc/meminfo ，是典型的kv值例子

```
#cat /proc/meminfo
MemTotal:        2008012 kB
MemFree:          104004 kB
MemAvailable:    1060412 kB
Buffers:          167316 kB
Cached:           877672 kB
SwapCached:            0 kB
Active:          1217032 kB
Inactive:         522236 kB
Active(anon):     694948 kB
Inactive(anon):      236 kB
Active(file):     522084 kB
Inactive(file):   522000 kB
……
```
对应处理代码说明，重点需要关注**readKV**函数实现。

```
local system = require("system")
require("class")
local CvProc = require("vproc")

local CkvProc = class("kvProc", CvProc)

function CkvProc:_init_(proto, pffi, pFile, tName)
    CvProc._init_(self, proto, pffi, pFile)   -- 从基础类继承
    self._protoTable = {
        line = tName,    -- 表名 如/proc/meminfo 可以取 meminfo 为表名
        ls = nil,
        vs = {}
    }
end

function CkvProc:checkTitle(title)   -- 去除label中的保留字符，防止数据保存失败
    local res = string.gsub(title, ":", "")  --去除 :和)
    res = string.gsub(res, "%)", "")
    res = string.gsub(res, "%(", "_")    --（替换为_
    return res
end

function CkvProc:readKV(line)   -- 处理单行数据
    local data = self._ffi.new("var_kvs_t")   -- 新增一个 var_kvs_t 结构体
    assert(self._cffi.var_input_kvs(self._ffi.string(line), data) == 0)   --调用c api 进行读取
    assert(data.no >= 1)   --确保访问成功

    local name = self._ffi.string(data.s)   -- 标题处理
    name = self:checkTitle(name)
    local value = tonumber(data.value[0])

    local cell = {name=name, value=value}  -- 生存一段数据
    table.insert(self._protoTable["vs"], cell)   -- 将数据存入表中
end

function CkvProc:proc(elapsed, lines)  --处理数据
    self._protoTable.vs = {}
    CvProc.proc(self)
    for line in io.lines(self.pFile) do   --遍历行
        self:readKV(line)   -- 处理数据
    end
    self:appendLine(self._protoTable)  -- 添加到大表中
    return self:push(lines)   --往外推送
end

return CkvProc
```



[返回目录](/guide/guide.md)