# 1 Tasktop功能测试
## 1.0 低负载状态

    $uptime
    11:13:52 up 783 days, 20:59,  0 users,  load average: 1.28, 10.09, 32.79

## 1.1 多线程、多进程场景
由于多线程或者多进程导致cpu资源不足，大量task在队列中无法被调度导致的R状态冲高
### 1.1.1 测试方法
通过stress工具 启动64个进程进行计算

    stress -c 64

### 1.1.2 测试结果
    2023-05-24 02:15:36
    UTIL&LOAD
    usr    sys iowait  load1      R      D   fork : proc 
    97.8    2.2    0.0   66.3     74      1    108 : cpuUsage.sh(3825) ppid=43661 cnt=18 
    [ cpu ]    usr    sys iowait  delay(ns)
    [cpu-0]   95.3    4.7    0.0 50851273279
    [cpu-1]   96.3    3.7    0.0 57819560956
    [cpu-2]  100.0    0.0    0.0 51718093440
    [cpu-3]   99.7    0.3    0.0 51741848156
    TASKTOP
            COMMAND    PID   PPID      START        RUN %UTIME %STIME   %CPU
            (stress)    332    325 1684894406        130    6.7    0.0    6.7
            (stress)    343    325 1684894406        130    6.7    0.0    6.7
            (stress)    328    325 1684894406        130    6.3    0.0    6.3
            (stress)    329    325 1684894406        130    6.3    0.0    6.3
            (stress)    330    325 1684894406        130    6.3    0.0    6.3
            (stress)    331    325 1684894406        130    6.3    0.0    6.3
            (stress)    326    325 1684894406        130    6.3    0.0    6.3

观察到load1迅速冲高，伴随系统以及per-cpu的cpu利用率打满，cpu时间集中于用户态，per-cpu的调度延迟达到50s。

## 1.2 cpu绑核场景
由于配置不当，导致大量进程堆积在少部分CPU核上导致的R状态进程数冲高
### 1.2.1 测试方法
启动64个计算进程 绑定到cpu0上

    cd tasktopSelftest
    make clean;make
    ./test bind
### 1.2.2 测试结果
    2023-05-24 02:49:31
    UTIL&LOAD
    usr    sys iowait  load1      R      D   fork : proc 
    27.0    2.3    0.0   61.2     66      1     70 : walle-plugin-no(51999) ppid=100868 cnt=6 
    [ cpu ]    usr    sys iowait  delay(ns)
    [cpu-0]  100.0    0.0    0.0 190259714471
    [cpu-1]    2.6    2.0    0.0   56072152
    [cpu-2]    2.7    4.3    0.0   95235540
    [cpu-3]    2.6    3.0    0.0   97010245
    TASKTOP
            COMMAND    PID   PPID      START        RUN %UTIME %STIME   %CPU
            (telegraf) 100590      1 1684745851     150720    1.7    1.3    3.0
                (test)  48438  48433 1684896441        130    1.7    0.0    1.7
                (test)  48440  48433 1684896441        130    1.7    0.0    1.7
                (test)  48443  48433 1684896441        130    1.7    0.0    1.7
                (test)  48444  48433 1684896441        130    1.7    0.0    1.7
                (test)  48449  48433 1684896441        130    1.7    0.0    1.7
                (test)  48451  48433 1684896441        130    1.7    0.0    1.7
观察到load1冲高，伴随有R状态进程数增多，但系统cpu利用率不高，cpu-0的利用率打满，cpu-0的调度延迟达到190s
## 1.3 大量fork场景
由于loadavg的采样是周期性的，可能存在大量短task在采样时出现但是无法被top等工具捕捉等情况
### 1.3.1 测试方法
主进程每1ms周期性的进行fork出128个进程 每个进程执行10w次自增运算后退出

    cd tasktopSelftest
    make clean;make
    ./test fork
### 1.3.2 测试结果

    2023-05-24 03:42:18
    UTIL&LOAD
    usr    sys iowait  load1      R      D   fork : proc 
    57.8   36.5    0.0   28.5     43      1  16671 : test(122383) ppid=64110 cnt=16607 
    [ cpu ]    usr    sys iowait  delay(ns)
    [cpu-0]   55.8   38.9    0.0 19125326036
    [cpu-1]   58.0   36.3    0.0 18447412733
    [cpu-2]   56.5   38.2    0.0 18997158534
    [cpu-3]   61.6   32.8    0.0 18552763236
    TASKTOP
            COMMAND    PID   PPID      START        RUN %UTIME %STIME   %CPU
                (test) 122383  64110 1684899622        116    1.3   46.7   48.0
            (telegraf) 100590      1 1684745851     153887    3.7    1.7    5.3
            (uniagent) 100622      1 1684745851     153887    2.0    0.3    2.3
            (tasktop)  27523  27482 1684899160        578    0.3    0.7    1.0
        (argusagent) 102026      1 1684745875     153863    0.3    0.3    0.7
        (ksoftirqd/3)     26      2 1617171267   67728471    0.0    0.3    0.3
            (systemd)      1      0 1617171267   67728471    0.0    0.3    0.3
                (node)  43661  43620 1684891252       8486    0.0    0.3    0.3
            (dfget)  56945      1 1684899541        197    0.3    0.0    0.3

观察到load增高，同时CPU利用率也跑满，存在较多R进程但是没有被top捕捉到。此时fork增量激增，fork调用次数最多的进程为test，同时test进程的sys利用率较高。
## 1.4 cgroup限流场景
与cpu核绑定类似，通过cgroup限制了cpu带宽导致task堆积在就绪队列
### 1.4.1 测试方法
创建一个cgroup 限定cgroup的cpu额度 启动一个进程并将task的pid加入cgroup的tasks中 之后该进程创建128个线程执行计算任务

    # 创建cgroup 设置限流30% 使用cpuset.cpus=0-3 
    cd /sys/fs/cgroup/cpu/
    mkdir stress_cg; cd stress_cg
    echo 100000 > cpu.cfs_period_us
    echo 30000 > cpu.cfs_quota_us

    # run test
    ./test multi_thread

### 1.4.2 测试结果

        [/sys/fs/cgroup/cpu/aegis/cpu.stat] nr_periods=4 nr_throttled=0 throttled_time=0 nr_burst=0 burst_time=0
        [/sys/fs/cgroup/cpu/docker/cpu.stat] nr_periods=0 nr_throttled=0 throttled_time=0 nr_burst=0 burst_time=0
        [/sys/fs/cgroup/cpu/infra.slice/cpu.stat] nr_periods=0 nr_throttled=0 throttled_time=0 nr_burst=0 burst_time=0
        [/sys/fs/cgroup/cpu/agent/cpu.stat] nr_periods=0 nr_throttled=0 throttled_time=0 nr_burst=0 burst_time=0
        [/sys/fs/cgroup/cpu/user.slice/cpu.stat] nr_periods=0 nr_throttled=0 throttled_time=0 nr_burst=0 burst_time=0
        [/sys/fs/cgroup/cpu/stress_cg/cpu.stat] nr_periods=18841 nr_throttled=18829 throttled_time=6629585264179 nr_burst=0 burst_time=0
        [/sys/fs/cgroup/cpu/system.slice/cpu.stat] nr_periods=0 nr_throttled=0 throttled_time=0 nr_burst=0 burst_time=0
        
        2023-05-24 08:20:52
        UTIL&LOAD
        usr    sys iowait  load1      R      D   fork : proc 
        12.6    2.2    0.0    1.2      2      1     76 : cpuUsage.sh(94513) ppid=3887 cnt=9 
        [ cpu ]    usr    sys iowait  delay(ns)
        [cpu-0]   13.6    2.7    0.0 2731979911
        [cpu-1]   12.0    2.3    0.0 45323759752
        [cpu-2]   13.3    2.7    0.0 8640595296
        [cpu-3]   11.6    1.7    0.0 286465968694
        TASKTOP
                COMMAND    PID   PPID      START        RUN %UTIME %STIME   %CPU
                (test)  20239  42752 1684914077       2375   40.0    0.3   40.3
            (AliYunDun)   5107      1 1684909031       7421    2.0    1.3    3.3
            (telegraf)   1455      1 1684908947       7505    1.7    1.3    3.0
            (uniagent)   1226      1 1684908945       7507    1.3    0.3    1.7
                (walle)   1166      1 1684908945       7507    1.0    0.0    1.0
                (node)   3887   3832 1684908988       7464    0.3    0.3    0.7
                (node)   3936   3832 1684908988       7464    0.3    0.3    0.7
                (java)   2360      1 1684908951       7501    0.0    0.3    0.3
    (logagent-collecS   2423   2347 1684908951         83    0.3    0.0    0.3
        (argusagent)   3096      1 1684908962       7490    0.0    0.3    0.3

可以观察到此时虽然**实际负载**很高，大量task由于限流处于R状态，但是由于cgroup机制task并不位于就绪队列中，因此R状态数量指标不准确导致load1计算不准（load1无法准确体现出系统的负载情况）。但是在cgroup限流信息中可以看到stress_cg中**出现了大量的限流**，并且**per-cpu的调度延迟很高**，一定程度体现了cpu就绪队列中存在task堆积。