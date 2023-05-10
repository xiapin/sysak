#  功能说明
continuous profiling 系统性能持续分析

使用说明:
```
Usage: sysak raptor [options] [args]
Options:
  oncpu
  offcpu
  pingslow
  userslow
args:
  --akid               akid
    SLS AccessKeyID

  --akse               akse
    SLS AccessKeySecret

  --app-name           
    application name used when uploading profiling data

  --cpu                -1
    Number of cpu you want to profile. -1 to profile the whole system

  --endpoint           endpoint
    SLS Endpoint

  --exit-time          2880
    time of minutes unit the profiling to exit, default  means 2 days

  --help               false
    help for oncpu

  --log-level          info
    log level: debug|info|warn|error

  --logstore           akid
    SLS Logstore

  --pid                -1
    PID you want to profile. -1 to profile the whole system

  --project            akid
    SLS Project

  --sample-rate        100
    sample rate for the profiler in Hz. 100 means reading 100 times per second

  --server             http://localhost:4040
    the server address

  --sls                unuser
    producer/procuderaw/consumer data to/from SLS

  --space              0
    0 profile user space, 1 user+kernel space

  --symbol-cache-size  256
    max size of symbols cache

  --tag                {}
    tag in key=value form. The flag may be specified multiple times

  --timer              -1
    Timer(min) trigger to start the profiling, minimum timer every 5 minutes

  --upload-rate        10s
    profile upload rate 

  --upload-threads     4
    number of upload threads

  --upload-timeout     10s
    profile upload timeout

  --usage              -1
    Cpu usage trigger to start the profiling
example:
sysak raptor oncpu --server "http://127.0.0.1:4040" --app-name 实例ip
```
- oncpu

主要用于排查应用偶现占用cpu过多的问题，对应ECS系统指标升高却不知道是哪个应用引起；

- offcpu

主要排查应用的性能问题，对应 应用自身的一些锁竞争等问题导致迟迟不能占用cpu，排查可能的问题如应用收包慢等问题；

- pingslow

ping主机有时延抖动，怀疑慢在主机的情况；

- userslow

用户进程没有及时收包造成的网络假抖动的情况；
**以oncpu功能为例，其他功能待完善**  

| 参数 | 说明 |
| --- | --- |
| --app-name | 标记profiling对象，一般取实例id |
| --pid | 指定任务做profiling，较少数据开销 |
| --sample-rate | 指定采样的频率，既1s内采样多少次 |
| --cpu | 指定cpu做profiilng，解决应用绑核，单cpu冲高问题 |
| --space | 采样上下文，0 进程上下文，1包含中断上下文 |
| --server | 服务端地址，local本地诊断结果输出 |
| --uplaod-rate | 多久上传一次数据，既数据切片的时间 |
| --exit-time | 默认部署后会运行两天退出，防止同学部署后忘记关闭，-1表示一直运行 |
| --akid | SLS AccessKeyID |
| --akse | SLS AccessKeySecret |
| --logstore | SLS Logstore |
| --project | SLS Project |
| --sls |  producer/procuderaw/consumer data to/from SLS |

云下环境后端flamegraph展示配合sysom使用。
