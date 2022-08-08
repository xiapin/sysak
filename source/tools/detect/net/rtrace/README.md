# 功能说明
基于 ebpf 实现的实时网络丢包、延迟、异常分析工具

# rtrace

rtrace 具有四个子功能，分别是：异常诊断、丢包诊断、延迟诊断及网络 SLI。具体可以通过命令 `sysak rtrace --help` 查看。

```shell
rtrace 0.1.0
Diagnosing tools of kernel network

USAGE:
    rtrace <SUBCOMMAND>

FLAGS:
    -h, --help       Prints help information
    -V, --version    Prints version information

SUBCOMMANDS:
    abnormal    Abnormal connection diagnosing
    drop        Packet drop diagnosing
    help        Prints this message or the help of the given subcommand (s)
    latency     Packet latency diagnosing
    sli         Collection machine sli

```

## rtrace abnormal: 异常链接诊断

rtrace abnormal 通过遍历机器上的 tcp 链接，并检查 tcp 链接的相关参数来判断哪些链接可能存在异常。


### 命令行参数解析

```shell
rtrace-abnormal 0.2.0
Abnormal connection diagnosing

USAGE:
    rtrace abnormal [OPTIONS]

FLAGS:
    -h, --help       Prints help information
    -V, --version    Prints version information

OPTIONS:
        --btf <btf>        Custom btf path
        --dst <dst>        Remote network address of traced sock
        --pid <pid>        Process identifier
        --proto <proto>    Network protocol type, now only support tcp [default: tcp]
        --sort <sort>      Sorting key, including: synq, acceptq, rcvm, sndm, drop, retran, ooo [default: acceptq]
        --src <src>        Local network address of traced sock
        --top <top>        Show top N connections [default: 10]
```

1. `--sort` 对参数进行排序，目前包括：

synq: 半连接队列长度
acceptq: 全连接队列长度
rcvm: 接收端内存
sndm: 发送端内存
drop: 丢包数
retran: 重传数
ooo: 乱序报文数，注不支持 centos 3.10


### 常用命令行示例

1. `rtrace abnormal`: 根据全连接队列长度（默认为 acceptq）进行排序，显示长度最长的前 10（默认为 Top10）条链接信息；

2. `rtrace abnormal --sort rcvm --top 5`: 根据接收端内存大小进行排序，显示使用内存最多的前 5 条链接；

## rtrace drop: packet drop diagnosing

rtrace drop 进行丢包诊断分析。

### 命令行参数解析

```
rtrace-drop 0.2.0
Packet drop diagnosing

USAGE:
    rtrace drop [FLAGS] [OPTIONS]

FLAGS:
        --conntrack    Enable conntrack modules
    -h, --help         Prints help information
        --iptables     Enable iptables modules
    -V, --version      Prints version information

OPTIONS:
        --btf <btf>          Custom btf path
        --period <period>    Period of display in seconds. 0 mean display immediately when event triggers [default: 3]
        --proto <proto>      Network protocol type [default: tcp]
```

1. `--conntrack`: 跟踪 conntrack 是否存在丢包；

2. `--iptables`: 跟踪 iptables 是否存在丢包；

### 常用命令行示例

1. `rtrace drop`: 跟踪丢包，默认每三秒种打印丢包信息，丢包信息包括：丢包堆栈、snmp、netstat、dev 等；

2. `rtrace drop --conntrack --iptables`: 除了命令 1 的功能外，还跟踪 conntrack 和 iptables 是否存在丢包；

## rtrace sli: data collections about sli

rtrace sli 采集网络相关的 sli 指标。

```
rtrace-sli 0.2.0
Collection machine sli

USAGE:
    rtrace sli [FLAGS] [OPTIONS]

FLAGS:
    -a, --applat     Collect latency between kernel and application in receiving side
        --drop       Collect drop metrics
    -h, --help       Prints help information
        --latency    Collect latency metrics
        --retran     Collect retransmission metrics
        --shell      Output every sli to shell
    -V, --version    Prints version information

OPTIONS:
        --period <period>          Data collection cycle, in seconds [default: 3]
        --threshold <threshold>    Max latency to trace, default is 1000ms [default: 1000]
```

### 命令行参数解析

1. `--applat`: 检测网络包从内核态到用户态的耗时；

2. `--latency`: 检测 tcp 链接 rtt，打印 rtt 分布直方图；

3. `--shell`: 打印输出到终端；

4. `--threshold`: 延迟阈值，超过该阈值会打印超时的链接具体信息；


### 常用命令行示例

1. `rtrace sli --latency --shell`: 打印 rtt 分布直方图，并打印 rtt 超过 1000ms 的 tcp 链接五元组信息；

2. `rtrace sli --latency --threshold 200 --shell`: 打印 rtt 分布直方图，并打印 rtt 超过 200ms 的 tcp 链接五元组信息；

3. `rtrace sli --latency --applat --shell`: 打印 rtt 分布直方图，并打印 rtt 或应用延迟超过 1000ms 的 tcp 链接五元组信息；
