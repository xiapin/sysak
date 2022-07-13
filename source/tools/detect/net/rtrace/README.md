# 功能说明
基于ebpf实现的实时网络丢包、延迟、异常分析工具

# rtrace

rtrace 具有四个子功能，分别是：异常诊断、丢包诊断、延迟诊断及网络SLI。具体可以通过命令 `sysak rtrace --help` 查看。

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
    help        Prints this message or the help of the given subcommand(s)
    latency     Packet latency diagnosing
    sli         Collection machine sli

```

## rtrace abnormal: Abnormal connection diagnosing

rtrace abnormal通过遍历机器上的tcp链接，并检查tcp链接的相关参数来判断哪些链接可能存在异常。

```shell
rtrace-abnormal 0.1.0
Abnormal connection diagnosing

USAGE:
    rtrace abnormal [OPTIONS]

FLAGS:
    -h, --help       Prints help information
    -V, --version    Prints version information

OPTIONS:
        --dst <dst>        Remote network address of traced sock
        --pid <pid>        Process identifier
        --proto <proto>    Network protocol type, now only support tcp [default: tcp]
        --src <src>        Local network address of traced sock
        --top <top>        Show top N connections [default: 10]
```

## rtrace drop: packet drop diagnosing

rtrace drop 进行丢包诊断分析。

```
rtrace-drop 0.1.0
Packet drop diagnosing

USAGE:
    rtrace drop [OPTIONS]

FLAGS:
    -h, --help       Prints help information
    -V, --version    Prints version information

OPTIONS:
        --proto <proto>    Network protocol type [default: tcp]
```


## rtrace latency: packet latency diagnosing

rtrace latency 进行网络包时延分析。

```
rtrace-latency 0.1.0
Packet latency diagnosing

USAGE:
    rtrace latency [OPTIONS]

FLAGS:
    -h, --help       Prints help information
    -V, --version    Prints version information

OPTIONS:
        --dst <dst>            Remote network address of traced sock
        --latency <latency>    The threshold of latency [default: 0]
        --pid <pid>            Process identifier
        --proto <proto>        Network protocol type [default: tcp]
        --src <src>            Local network address of traced sock
```

## rtrace sli: data collections about sli

rtrace sli 采集网络相关的sli指标。

```
rtrace-sli 0.1.0
Collection machine sli

USAGE:
    rtrace sli [FLAGS] [OPTIONS]

FLAGS:
    -h, --help       Prints help information
        --retran     Collect retransmission metrics
    -V, --version    Prints version information

OPTIONS:
        --period <period>    Data collection cycle, in seconds [default: 3]
```