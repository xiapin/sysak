# Tasktop 测试文档

## 1. 测试环境

    $lscpu
    Architecture:          x86_64
    CPU op-mode(s):        32-bit, 64-bit
    Byte Order:            Little Endian
    CPU(s):                4
    On-line CPU(s) list:   0-3
    Thread(s) per core:    2
    Core(s) per socket:    1
    Socket(s):             2
    NUMA node(s):          1
    Vendor ID:             GenuineIntel
    CPU family:            6
    Model:                 79
    Model name:            Intel(R) Xeon(R) CPU E5-2682 v4 @ 2.50GHz
    Stepping:              1
    CPU MHz:               2494.224
    BogoMIPS:              4988.44
    Hypervisor vendor:     KVM
    Virtualization type:   full
    L1d cache:             32K
    L1i cache:             32K
    L2 cache:              256K
    L3 cache:              40960K
    NUMA node0 CPU(s):     0-3
    Flags:                 fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ss ht syscall nx pdpe1gb rdtscp lm constant_tsc rep_good nopl nonstop_tsc cpuid tsc_known_freq pni pclmulqdq ssse3 fma cx16 pcid sse4_1 sse4_2 x2apic movbe popcnt tsc_deadline_timer aes xsave avx f16c rdrand hypervisor lahf_lm abm 3dnowprefetch invpcid_single fsgsbase tsc_adjust bmi1 hle avx2 smep bmi2 erms invpcid rtm rdseed adx smap xsaveopt

## 2. Tasktop功能测试

### 2.0 低负载状态

    $uptime
    11:13:52 up 783 days, 20:59,  0 users,  load average: 1.28, 10.09, 32.79

### 2.1 多线程、多进程场景

由于多线程或者多进程导致cpu资源不足，大量task在队列中无法被调度导致的R状态冲高

#### 2.1.1 测试方法

通过stress工具 启动64个进程进行计算

    stress -c 64

#### 2.1.2 测试结果

    [TIME-STAMP] 2023-06-06 02:59:25
    [UTIL&LOAD]
    usr    sys iowait  load1      R      D   fork : proc 
    94.5    5.5    0.0   56.7     72      1    203 : cpuUsage.sh(56709) ppid=71957 cnt=15 
    [PER-CPU]
        cpu    usr    sys   nice   idle iowait  h-irq  s-irq  steal  delay(ms)
    cpu-0   92.6    7.4    0.0    0.0    0.0    0.0    0.0    0.0      74125
    cpu-1   93.4    6.4    0.0    0.0    0.0    0.0    0.3    0.0      72258
    cpu-2   97.2    2.8    0.0    0.0    0.0    0.0    0.0    0.0      68874
    cpu-3   94.7    5.3    0.0    0.0    0.0    0.0    0.0    0.0      68119
    [TASKTOP]
            COMMAND    PID   PPID      START        RUN %UTIME %STIME   %CPU
            (cpptools)  72168  72010 1686014764       5601    9.7    0.3   10.0
        (AliYunDun)  26698      1 1685936915      83450    5.3    2.7    8.0
            (stress)  54005  53998 1686020277         88    7.3    0.0    7.3
            (stress)  54011  53998 1686020277         88    7.3    0.0    7.3
                                    ................
            (stress)  54062  53998 1686020277         88    6.7    0.0    6.7
            (stress)  54037  53998 1686020277         88    6.7    0.0    6.7
            (stress)  54051  53998 1686020277         88    6.3    0.0    6.3
            (telegraf)  38654      1 1684980656    1039709    4.0    2.0    6.0
                (node)  72010  71892 1686014761       5604    2.3    2.0    4.3
            (uniagent)  39728      1 1684980670    1039695    1.3    0.3    1.7
            (tasktop)  56641  56640 1686020359          6    0.3    1.3    1.7
        (argusagent)  39850      1 1684980673    1039692    0.7    0.7    1.3
                (node)  71957  71892 1686014761       5604    0.3    0.7    1.0
                (node)  71892  71871 1686014761       5604    0.7    0.0    0.7
            (systemd)      1      0 1684918982    1101383    0.3    0.3    0.7
        (staragentd)  40538      1 1684980691    1039674    0.0    0.3    0.3
        (rcu_sched)     10      2 1684918982    1101383    0.0    0.3    0.3
            (ilogtail)  38538   1620 1684980652    1039713    0.3    0.0    0.3
        (syslog-ng)  38653      1 1684980656    1039709    0.3    0.0    0.3
        (dbus-daemon)   1125      1 1684918989    1101376    0.3    0.0    0.3
    (systemd-journalS  38655      1 1684980656    1039709    0.3    0.0    0.3
                (sshd)  71789  71756 1686014760       5605    0.0    0.3    0.3
            (logagent)  39295      1 1684980663    1039702    0.3    0.0    0.3
            (dockerd)   1426      1 1684918991    1101374    0.3    0.0    0.3
    (AliYunDunUpdateS   1793      1 1684918991    1101374    0.3    0.0    0.3
            (walle)  40189      1 1684980677    1039688    0.3    0.0    0.3
    [D-STASK]
            COMMAND    PID   PPID  STACK
        (load_calc)    141    141 [<0>] load_calc_func+0x57/0x130
                                    [<0>] kthread+0xf5/0x130
                                    [<0>] ret_from_fork+0x1f/0x30
                                    [<0>] 0xffffffffffffffff
    WARN: CPU overall utilization is high.

观察到load1迅速冲高，伴随系统以及per-cpu的cpu利用率打满，cpu时间集中于用户态，per-cpu的调度延迟达到60s。

### 2.2 cpu绑核场景

由于配置不当，导致大量进程堆积在少部分CPU核上导致的R状态进程数冲高

#### 2.2.1 测试方法

启动64个计算进程 绑定到cpu0上

    cd tasktopSelftest
    make clean;make
    ./test bind

#### 2.2.2 测试结果

    [TIME-STAMP] 2023-06-06 03:00:54
    [UTIL&LOAD]
    usr    sys iowait  load1      R      D   fork : proc 
    28.5    3.1    0.0   67.7     64      2     80 : logagentctl.sh(59611) ppid=59608 cnt=6 
    [PER-CPU]
        cpu    usr    sys   nice   idle iowait  h-irq  s-irq  steal  delay(ms)
    cpu-0  100.0    0.0    0.0    0.0    0.0    0.0    0.0    0.0     191516
    cpu-1    3.3    2.6    0.0   91.7    0.0    0.0    2.3    0.0         91
    cpu-2    4.3    4.0    0.0   91.6    0.0    0.0    0.0    0.0        154
    cpu-3    5.0    6.0    0.0   89.0    0.0    0.0    0.0    0.0        129
    [TASKTOP]
            COMMAND    PID   PPID      START        RUN %UTIME %STIME   %CPU
        (AliYunDun)  26698      1 1685936915      83539    2.3    1.7    4.0
            (telegraf)  38654      1 1684980656    1039798    1.3    2.0    3.3
                (node)  72010  71892 1686014761       5693    2.0    0.7    2.7
                (test)  58683  58682 1686020436         18    1.7    0.0    1.7
                (test)  58684  58682 1686020436         18    1.7    0.0    1.7
                                        ...........
                (test)  58744  58682 1686020436         18    1.7    0.0    1.7
                (test)  58745  58682 1686020436         18    1.7    0.0    1.7
                (test)  58746  58682 1686020436         18    1.7    0.0    1.7
            (tasktop)  56641  56640 1686020359         95    0.3    1.3    1.7
                (test)  58725  58682 1686020436         18    1.3    0.0    1.3
                (test)  58702  58682 1686020436         18    1.3    0.0    1.3
            (uniagent)  39728      1 1684980670    1039784    1.0    0.3    1.3
                (test)  58696  58682 1686020436         18    1.3    0.0    1.3
                (test)  58697  58682 1686020436         18    1.3    0.0    1.3
                                        .............
                (test)  58722  58682 1686020436         18    1.3    0.0    1.3
                (test)  58723  58682 1686020436         18    1.3    0.0    1.3
                (test)  58685  58682 1686020436         18    1.3    0.0    1.3
            (dockerd)   1426      1 1684918991    1101463    1.0    0.0    1.0
        (staragentd)  40538      1 1684980691    1039763    0.3    0.7    1.0
                (sshd)  71789  71756 1686014760       5694    0.3    0.3    0.7
        (argusagent)  39850      1 1684980673    1039781    0.3    0.3    0.7
    (systemd-journalS  38655      1 1684980656    1039798    0.3    0.0    0.3
    (systemd-logind)S   1159      1 1684918989    1101465    0.0    0.3    0.3
            (ilogtail)  38538   1620 1684980652    1039802    0.0    0.3    0.3
            (walle)  40189      1 1684980677    1039777    0.3    0.0    0.3
        (syslog-ng)  38653      1 1684980656    1039798    0.0    0.3    0.3
                (node)  71892  71871 1686014761       5693    0.3    0.0    0.3
                (node)  71957  71892 1686014761       5693    0.0    0.3    0.3
        (dbus-daemon)   1125      1 1684918989    1101465    0.3    0.0    0.3
    [D-STASK]
            COMMAND    PID   PPID  STACK
        (load_calc)    141    141 [<0>] load_calc_func+0x57/0x130
                                    [<0>] kthread+0xf5/0x130
                                    [<0>] ret_from_fork+0x1f/0x30
                                    [<0>] 0xffffffffffffffff
    WARN: Some tasks bind cpu. Please check cpu:  [0]

观察到load1冲高，伴随有R状态进程数增多，但系统cpu利用率不高，cpu-0的利用率打满，cpu-0的调度延迟达到190s

### 2.3 大量fork场景

由于loadavg的采样是周期性的，可能存在大量短task在采样时出现但是无法被top等工具捕捉等情况

#### 2.3.1 测试方法

主进程每1ms周期性的进行fork出128个进程 每个进程执行10w次自增运算后退出

    cd tasktopSelftest
    make clean;make
    ./test fork

#### 2.3.2 测试结果

    [TIME-STAMP] 2023-06-06 03:02:38
    [UTIL&LOAD]
    usr    sys iowait  load1      R      D   fork : proc 
    57.7   36.0    0.0   57.0     72      1  14614 : test(62171) ppid=49997 cnt=14548 
    [PER-CPU]
        cpu    usr    sys   nice   idle iowait  h-irq  s-irq  steal  delay(ms)
    cpu-0   58.9   33.7    0.0    6.8    0.0    0.0    0.3    0.3      24100
    cpu-1   61.4   33.1    0.0    5.1    0.0    0.0    0.3    0.0      24844
    cpu-2   56.0   37.5    0.0    6.5    0.0    0.0    0.0    0.0      24961
    cpu-3   54.5   39.3    0.0    6.2    0.0    0.0    0.0    0.0      25138
    [TASKTOP]
            COMMAND    PID   PPID      START        RUN %UTIME %STIME   %CPU
        (AliYunDun)  26698      1 1685936915      83643   39.0   15.3   54.3
                (test)  62171  49997 1686020542         16    1.7   40.0   41.7
            (telegraf)  38654      1 1684980656    1039902    2.0    1.7    3.7
                (node)  72010  71892 1686014761       5797    2.0    1.3    3.3
            (uniagent)  39728      1 1684980670    1039888    2.0    0.0    2.0
            (tasktop)  56641  56640 1686020359        199    0.3    1.3    1.7
        (argusagent)  39850      1 1684980673    1039885    0.7    0.7    1.3
    (kworker/3:3-eveIts)  56014      2 1686020335        223    0.0    1.0    1.0
    (kworker/0:0-eveIts)  36226      2 1686019771        787    0.0    1.0    1.0
    (kworker/1:0-eveIts)  34141      2 1686019712        846    0.0    1.0    1.0
            (walle)  40189      1 1684980677    1039881    1.0    0.0    1.0
            (ilogtail)  38538   1620 1684980652    1039906    0.3    0.3    0.7
    (kworker/2:0-eveIts)  99018      2 1686020550          8    0.0    0.7    0.7
                (java)   2234      1 1685929891      90667    0.3    0.0    0.3
        (staragentd)  40538      1 1684980691    1039867    0.0    0.3    0.3
        (rcu_sched)     10      2 1684918982    1101576    0.0    0.3    0.3
            (logagent)  39295      1 1684980663    1039895    0.3    0.0    0.3
    (logagent-collecS  39368  39295 1684980663    1039895    0.3    0.0    0.3
                (sshd)  71789  71756 1686014760       5798    0.0    0.3    0.3
                (node)  71892  71871 1686014761       5797    0.3    0.0    0.3
                (node)  71957  71892 1686014761       5797    0.3    0.0    0.3
        (ksoftirqd/2)     21      2 1684918982    1101576    0.0    0.3    0.3
    (AliYunDunUpdateS   1793      1 1684918991    1101567    0.0    0.3    0.3
    [D-STASK]
            COMMAND    PID   PPID  STACK
        (load_calc)    141    141 [<0>] load_calc_func+0x57/0x130
                                    [<0>] kthread+0xf5/0x130
                                    [<0>] ret_from_fork+0x1f/0x30
                                    [<0>] 0xffffffffffffffff
    WARN: CPU overall utilization is high.
    INFO: Sys time of cpu is high.

观察到load增高，同时CPU利用率也跑满，存在较多R进程但是没有被top捕捉到。此时fork增量激增，fork调用次数最多的进程为test，同时test进程的sys利用率较高。

### 2.4 cgroup限流场景

与cpu核绑定类似，通过cgroup限制了cpu带宽导致task堆积在就绪队列

#### 2.4.1 测试方法

创建一个cgroup，限定cgroup的cpu额度。启动一个进程并将task的pid加入cgroup的tasks中，之后该进程在30s后创建128个线程执行计算任务。

    # 创建cgroup 设置限流30% 使用cpuset.cpus=0-3 
    cd /sys/fs/cgroup/cpu/
    mkdir stress_cg; cd stress_cg
    echo 100000 > cpu.cfs_period_us
    echo 30000 > cpu.cfs_quota_us

    # run test
    ./test multi_thread
    echo pid > tasks

#### 2.4.2 测试结果

    [TIME-STAMP] 2023-06-06 03:05:57
    [UTIL&LOAD]
    usr    sys iowait  load1      R      D   fork : proc 
    11.4    2.5    0.0    5.8      1      1     93 : hostinfo(59107) ppid=40018 cnt=8 
    [PER-CPU]
        cpu    usr    sys   nice   idle iowait  h-irq  s-irq  steal  delay(ms)
    cpu-0   10.9    2.3    0.0   86.8    0.0    0.0    0.0    0.0     115336
    cpu-1   11.8    3.0    0.0   84.9    0.0    0.0    0.3    0.0     112956
    cpu-2   11.4    2.6    0.0   85.7    0.0    0.0    0.0    0.3      76902
    cpu-3   11.8    2.6    0.0   85.3    0.0    0.0    0.0    0.3      76995
    [CGROUP]
            cgroup_name      nr_periods    nr_throttled  throttled_time        nr_burst      burst_time
            stress_cg              31              31     11437051623               0               0
    [TASKTOP]
            COMMAND    PID   PPID      START        RUN %UTIME %STIME   %CPU
                (test)  55827  41984 1686020648        109   30.0    0.0   30.0
            (telegraf)  38654      1 1684980656    1040101    3.3    1.3    4.7
            (uniagent)  39728      1 1684980670    1040087    2.0    0.3    2.3
                (node)  72010  71892 1686014761       5996    1.3    1.0    2.3
            (tasktop)  56641  56640 1686020359        398    0.3    1.3    1.7
        (staragentd)  40538      1 1684980691    1040066    0.0    0.7    0.7
        (dbus-daemon)   1125      1 1684918989    1101768    0.3    0.3    0.7
    (systemd-logind)S   1159      1 1684918989    1101768    0.3    0.3    0.7
                (node)  71892  71871 1686014761       5996    0.7    0.0    0.7
            (walle)  40189      1 1684980677    1040080    0.3    0.3    0.7
            (chronyd)   3259      1 1684919007    1101750    0.0    0.3    0.3
                (java)  40355      1 1684980680    1040077    0.0    0.3    0.3
        (rcu_sched)     10      2 1684918982    1101775    0.0    0.3    0.3
                (java)  41200      1 1684980696    1040061    0.3    0.0    0.3
    (systemd-journalS  38655      1 1684980656    1040101    0.0    0.3    0.3
    (logagent-collecS  39368  39295 1684980663    1040094    0.3    0.0    0.3
                (java)   2234      1 1685929891      90866    0.0    0.3    0.3
        (argusagent)  39850      1 1684980673    1040084    0.0    0.3    0.3
    [D-STASK]
            COMMAND    PID   PPID  STACK
        (load_calc)    141    141 [<0>] load_calc_func+0x57/0x130
                                    [<0>] kthread+0xf5/0x130
                                    [<0>] ret_from_fork+0x1f/0x30
                                    [<0>] 0xffffffffffffffff
    INFO: Load is normal.

可以观察到此时虽然**实际负载**很高，大量task由于限流处于R状态，但是由于cgroup机制task并不位于就绪队列中，因此R状态数量指标不准确导致load1计算不准（load1无法准确体现出系统的负载情况）。但是在cgroup限流信息中可以看到stress_cg中**出现了大量的限流**，并且**per-cpu的调度延迟很高**，一定程度体现了cpu就绪队列中存在task堆积。

### 2.5 IO打满场景

出现D状态的情况很多，最常见的是由于IO导致进入Uninterrupted Sleep状态。

#### 2.5.1 测试方法

利用stress工具开启64个进程将IO打满。

    stress -i 64

#### 2.5.2 测试结果

    [TIME-STAMP] 2023-06-06 03:06:55
    [UTIL&LOAD]
    usr    sys iowait  load1      R      D   fork : proc 
    12.0   67.2   16.0   75.7     37     65     96 : walle(40192) ppid=1 cnt=6 
    [PER-CPU]
        cpu    usr    sys   nice   idle iowait  h-irq  s-irq  steal  delay(ms)
    cpu-0   13.5   64.3    0.0    4.8   17.4    0.0    0.0    0.0      16684
    cpu-1   12.0   66.0    0.0    5.5   16.5    0.0    0.0    0.0     250220
    cpu-2   10.4   72.8    0.0    3.4   13.4    0.0    0.0    0.0     100659
    cpu-3   12.3   66.0    0.0    5.2   16.5    0.0    0.0    0.0      94436
    [CGROUP]
            cgroup_name      nr_periods    nr_throttled  throttled_time        nr_burst      burst_time
            stress_cg              31              31      9558428752               0               0
    [TASKTOP]
            COMMAND    PID   PPID      START        RUN %UTIME %STIME   %CPU
                (test)  55827  41984 1686020648        167   29.3    1.7   31.0
            (telegraf)  38654      1 1684980656    1040159    3.0    2.3    5.3
    (kworker/2:1H-kbIock    644      2 1684918988    1101827    0.0    4.7    4.7
        (AliYunDun)  59678      1 1686020777         38    2.3    1.7    4.0
            (stress)  60016  60015 1686020788         27    0.0    4.0    4.0
            (stress)  60017  60015 1686020788         27    0.0    4.0    4.0
            (stress)  60018  60015 1686020788         27    0.0    4.0    4.0
                            ..................
            (stress)  60076  60015 1686020788         27    0.0    3.7    3.7
            (stress)  60033  60015 1686020788         27    0.0    3.7    3.7
            (stress)  60078  60015 1686020788         27    0.0    3.7    3.7
            (stress)  60079  60015 1686020788         27    0.0    3.7    3.7
            (tasktop)  56641  56640 1686020359        456    0.0    1.7    1.7
            (uniagent)  39728      1 1684980670    1040145    1.3    0.3    1.7
        (jbd2/vda2-8)    606      2 1684918988    1101827    0.0    1.7    1.7
                (node)  72010  71892 1686014761       6054    1.3    0.3    1.7
                (node)  71892  71871 1686014761       6054    1.0    0.0    1.0
            (dockerd)   1426      1 1684918991    1101824    1.0    0.0    1.0
        (staragentd)  40538      1 1684980691    1040124    0.3    0.3    0.7
    (kworker/u8:0-flIsh-  55249      2 1686020629        186    0.0    0.3    0.3
    (systemd-journalS  38655      1 1684980656    1040159    0.3    0.0    0.3
            (systemd)      1      0 1684918982    1101833    0.0    0.3    0.3
        (argusagent)  39850      1 1684980673    1040142    0.3    0.0    0.3
        (dbus-daemon)   1125      1 1684918989    1101826    0.3    0.0    0.3
                (java)  41200      1 1684980696    1040119    0.0    0.3    0.3
                (node)  71957  71892 1686014761       6054    0.3    0.0    0.3
    (kworker/u8:3-flIsh-  46817      2 1686020071        744    0.0    0.3    0.3
    [D-STASK]
            COMMAND    PID   PPID  STACK
        (load_calc)    141    141 [<0>] load_calc_func+0x57/0x130
                                    [<0>] kthread+0xf5/0x130
                                    [<0>] ret_from_fork+0x1f/0x30
                                    [<0>] 0xffffffffffffffff
        (jbd2/vda2-8)    606    606 [<0>] jbd2_journal_commit_transaction+0x1356/0x1b60 [jbd2]
                                    [<0>] kjournald2+0xc5/0x260 [jbd2]
                                    [<0>] kthread+0xf5/0x130
                                    [<0>] ret_from_fork+0x1f/0x30
                                    [<0>] 0xffffffffffffffff
            (stress)  60022  60022 [<0>] submit_bio_wait+0x84/0xc0
                                    [<0>] blkdev_issue_flush+0x7c/0xb0
                                    [<0>] ext4_sync_fs+0x158/0x1e0 [ext4]
                                    [<0>] iterate_supers+0xb3/0x100
                                    [<0>] ksys_sync+0x60/0xb0
                                    [<0>] __ia32_sys_sync+0xa/0x10
                                    [<0>] do_syscall_64+0x55/0x1a0
                                    [<0>] entry_SYSCALL_64_after_hwframe+0x44/0xa9
                                    [<0>] 0xffffffffffffffff
                            ...................
            (stress)  60042  60042 [<0>] submit_bio_wait+0x84/0xc0
                                    [<0>] blkdev_issue_flush+0x7c/0xb0
                                    [<0>] ext4_sync_fs+0x158/0x1e0 [ext4]
                                    [<0>] iterate_supers+0xb3/0x100
                                    [<0>] ksys_sync+0x60/0xb0
                                    [<0>] __ia32_sys_sync+0xa/0x10
                                    [<0>] do_syscall_64+0x55/0x1a0
                                    [<0>] entry_SYSCALL_64_after_hwframe+0x44/0xa9
                                    [<0>] 0xffffffffffffffff
    WARN: CPU overall utilization is high.
    INFO: Sys time of cpu is high.
    WARN: The most stack, times=17
    [<0>] submit_bio_wait+0x84/0xc0
    [<0>] blkdev_issue_flush+0x7c/0xb0
    [<0>] ext4_sync_fs+0x158/0x1e0 [ext4]
    [<0>] iterate_supers+0xb3/0x100
    [<0>] ksys_sync+0x60/0xb0
    [<0>] __ia32_sys_sync+0xa/0x10
    [<0>] do_syscall_64+0x55/0x1a0
    [<0>] entry_SYSCALL_64_after_hwframe+0x44/0xa9
    [<0>] 0xffffffffffffffff

可以看到当前存在大量D状态task，并且系统的sys较高，并且抓取到了D-task的内核栈，主要是由stress进程在进行IO导致。可以看到聚合的stack信息，大量的task阻塞在submit_bio_wait。

### 2.6 模拟锁竞争场景

D状态在内核中出现的路径很多，锁的竞争是其中的一种情况，当有task取得锁后但是没有尽快释放就有可能导致出现大量D-task等待锁资源。

#### 2.6.1 测试方法

编写一个linux kernel module，在模块中创建一个proc文件，设置文件的open回调，在open时去获取mutex，拿到锁后sleep 10s，让其他尝试open该文件的task进入D状态。

    cd tasktop/tasktopSelftest/mod
    make
    make install
    bash race.sh

其中race.sh 启动10个task去调用cat命令，open proc文件。

#### 2.6.2 测试结果

    [TIME-STAMP] 2023-06-06 03:12:29
    [UTIL&LOAD]
    usr    sys iowait  load1      R      D   fork : proc 
    4.0    3.7    0.0   10.1      0     11    120 : bash(71118) ppid=41984 cnt=10 
    [PER-CPU]
        cpu    usr    sys   nice   idle iowait  h-irq  s-irq  steal  delay(ms)
    cpu-0    4.0    3.6    0.0   92.1    0.0    0.0    0.3    0.0         65
    cpu-1    4.3    3.7    0.0   92.0    0.0    0.0    0.0    0.0         58
    cpu-2    3.7    4.7    0.0   91.7    0.0    0.0    0.0    0.0         67
    cpu-3    4.0    2.7    0.0   93.0    0.0    0.0    0.0    0.3         60
    [TASKTOP]
            COMMAND    PID   PPID      START        RUN %UTIME %STIME   %CPU
        (AliYunDun)  59678      1 1686020777        372    2.7    2.0    4.7
            (telegraf)  38654      1 1684980656    1040493    1.0    2.3    3.3
            (uniagent)  39728      1 1684980670    1040479    1.7    1.0    2.7
                (node)  71892  71871 1686014761       6388    1.7    0.3    2.0
                (node)  72010  71892 1686014761       6388    1.3    0.3    1.7
            (tasktop)  56641  56640 1686020359        790    0.3    1.0    1.3
                (node)  71957  71892 1686014761       6388    0.3    0.7    1.0
            (walle)  40189      1 1684980677    1040472    0.7    0.3    1.0
        (staragentd)  40538      1 1684980691    1040458    0.3    0.3    0.7
            (ilogtail)  38538   1620 1684980652    1040497    0.3    0.0    0.3
                (java)  40355      1 1684980680    1040469    0.3    0.0    0.3
        (syslog-ng)  38653      1 1684980656    1040493    0.3    0.0    0.3
                (bash)  41984  71957 1686019937       1212    0.3    0.0    0.3
            (systemd)      1      0 1684918982    1102167    0.3    0.0    0.3
    (systemd-journalS  38655      1 1684980656    1040493    0.0    0.3    0.3
                (sshd)  71789  71756 1686014760       6389    0.0    0.3    0.3
            (logagent)  39295      1 1684980663    1040486    0.3    0.0    0.3
    (AliYunDunUpdateS   1793      1 1684918991    1102158    0.0    0.3    0.3
        (argusagent)  39850      1 1684980673    1040476    0.0    0.3    0.3
    [D-STASK]
            COMMAND    PID   PPID  STACK
        (load_calc)    141    141 [<0>] load_calc_func+0x57/0x130
                                    [<0>] kthread+0xf5/0x130
                                    [<0>] ret_from_fork+0x1f/0x30
                                    [<0>] 0xffffffffffffffff
                (cat)  71119  71119 [<0>] open_proc+0x2d/0x7e [mutex_block]
                                    [<0>] proc_reg_open+0x72/0x130
                                    [<0>] do_dentry_open+0x23a/0x3a0
                                    [<0>] path_openat+0x768/0x13e0
                                    [<0>] do_filp_open+0x99/0x110
                                    [<0>] do_sys_open+0x12e/0x210
                                    [<0>] do_syscall_64+0x55/0x1a0
                                    [<0>] entry_SYSCALL_64_after_hwframe+0x44/0xa9
                                    [<0>] 0xffffffffffffffff
                        ..................
                (cat)  71128  71128 [<0>] open_proc+0x2d/0x7e [mutex_block]
                                    [<0>] proc_reg_open+0x72/0x130
                                    [<0>] do_dentry_open+0x23a/0x3a0
                                    [<0>] path_openat+0x768/0x13e0
                                    [<0>] do_filp_open+0x99/0x110
                                    [<0>] do_sys_open+0x12e/0x210
                                    [<0>] do_syscall_64+0x55/0x1a0
                                    [<0>] entry_SYSCALL_64_after_hwframe+0x44/0xa9
                                    [<0>] 0xffffffffffffffff
    WARN: The most stack, times=8
    [<0>] open_proc+0x2d/0x7e [mutex_block]
    [<0>] proc_reg_open+0x72/0x130
    [<0>] do_dentry_open+0x23a/0x3a0
    [<0>] path_openat+0x768/0x13e0
    [<0>] do_filp_open+0x99/0x110
    [<0>] do_sys_open+0x12e/0x210
    [<0>] do_syscall_64+0x55/0x1a0
    [<0>] entry_SYSCALL_64_after_hwframe+0x44/0xa9
    [<0>] 0xffffffffffffffff

                                    
可以看到抓到的D状态task的命令、ppid、pid等信息，以及这些task当前的内核栈。其中最多的阻塞调用栈位于open_proc函数。

## 3. Tasktop性能测试

tasktop在运行时会对/proc文件系统进行遍历，采集相关信息，大量进程下可能会影响业务，因此对tasktop在不同进程数的场景下进行性能测试。

### 3.1 测试方法

创建N个进程，并让这N个进程进入sleep状态，不占用CPU资源，只增加proc文件数量。
./test sleep

### 3.2 测试结果

| Process Number   | CPU Utilization    |
| :---------: | :---------: |
| 147         | 0.3%        |
| 1155        | 2.0%        |
| 2157        | 4.3%        |
| 4147        | 8.3%        |
| 8152        | 11%-20%     |
| 12198       | 24%-33%   |
| 15161       | 27%-37%   |
| 20173       | 30%-59%     |

在存在20000个进程proc文件情况下，tasktop的整体cpu资源平均消耗估计在单核的45%左右。
