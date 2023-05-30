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

    [TIME-STAMP] 2023-05-30 01:37:05
    [UTIL&LOAD]
    usr    sys iowait  load1      R      D   fork : proc 
    96.4    3.5    0.0   63.2     71      1    124 : logagentctl.sh(33155) ppid=33152 cnt=6 
    [PER_CPU]
        cpu    usr    sys iowait  delay(ns)
    cpu-0   95.6    4.4    0.0 71161526462
    cpu-1   95.3    4.4    0.0 63930165092
    cpu-2   97.8    2.2    0.0 59050351997
    cpu-3   96.7    3.0    0.0 61105725553
    [TASKTOP]
            COMMAND    PID   PPID      START        RUN %UTIME %STIME   %CPU
            (stress)  32171  32170 1685410602         23    7.0    0.0    7.0
            (stress)  32172  32170 1685410602         23    7.0    0.0    7.0
            (stress)  32173  32170 1685410602         23    7.0    0.0    7.0
            (stress)  32174  32170 1685410602         23    7.0    0.0    7.0
            (stress)  32175  32170 1685410602         23    7.0    0.0    7.0

观察到load1迅速冲高，伴随系统以及per-cpu的cpu利用率打满，cpu时间集中于用户态，per-cpu的调度延迟达到60s。

### 2.2 cpu绑核场景

由于配置不当，导致大量进程堆积在少部分CPU核上导致的R状态进程数冲高

#### 2.2.1 测试方法

启动64个计算进程 绑定到cpu0上

    cd tasktopSelftest
    make clean;make
    ./test bind

#### 2.2.2 测试结果

    [TIME-STAMP] 2023-05-30 01:35:43
    [UTIL&LOAD]
    usr    sys iowait  load1      R      D   fork : proc 
    32.9   4.0    0.0   65.2     64      1    114 : cpuUsage.sh(30412) ppid=1755 cnt=15 
    [PER_CPU]
      cpu    usr    sys iowait  delay(ns)
    cpu-0  100.0    0.0    0.0 191098577265
    cpu-1   10.0    4.3    0.0  217842891
    cpu-2   11.0    6.6    0.0  152909467
    cpu-3   10.0    5.0    0.0  212943380
    [TASKTOP]
           COMMAND    PID   PPID      START        RUN %UTIME %STIME   %CPU
            (node)   1816   1710 1685409611        932   16.3    1.3   17.7
       (AliYunDun)  27299      1 1685004504     406039    3.0    2.0    5.0
            (node)   1710   1697 1685409611        932    2.3    0.7    3.0
        (telegraf)  38654      1 1684980656     429887    1.7    1.3    3.0
            (test)  20079  20076 1685410217        326    1.7    0.0    1.7
            (test)  20080  20076 1685410217        326    1.7    0.0    1.7
            (test)  20081  20076 1685410217        326    1.7    0.0    1.7
            (test)  20082  20076 1685410217        326    1.7    0.0    1.7
观察到load1冲高，伴随有R状态进程数增多，但系统cpu利用率不高，cpu-0的利用率打满，cpu-0的调度延迟达到190s

### 2.3 大量fork场景

由于loadavg的采样是周期性的，可能存在大量短task在采样时出现但是无法被top等工具捕捉等情况

#### 2.3.1 测试方法

主进程每1ms周期性的进行fork出128个进程 每个进程执行10w次自增运算后退出

    cd tasktopSelftest
    make clean;make
    ./test fork

#### 2.3.2 测试结果

    [TIME-STAMP] 2023-05-30 01:38:51
    [UTIL&LOAD]
     usr    sys iowait  load1      R      D   fork : proc 
    60.5   33.6    0.0   49.6     78      1  16304 : test(34338) ppid=1893 cnt=16230 
    [PER_CPU]
        cpu    usr    sys iowait  delay(ns)
    cpu-0   58.2   35.3    0.0 24573513865
    cpu-1   60.5   34.0    0.0 24758192280
    cpu-2   60.9   33.2    0.0 24491136783
    cpu-3   62.3   31.8    0.0 24525447957
    [TASKTOP]
                 COMMAND    PID   PPID      START        RUN %UTIME %STIME   %CPU
                  (test)  34338   1893 1685410675         56    1.3   41.0   42.3
              (telegraf)  38654      1 1684980656     430075    1.7    1.7    3.3
                  (node)   1816   1710 1685409611       1120    1.7    1.0    2.7
               (tasktop)  29995  29987 1685410527        204    0.0    1.3    1.3
              (uniagent)  39728      1 1684980670     430061    1.0    0.3    1.3
    (kworker/3:3-eveIts)  50231      2 1685410706         25    0.0    1.3    1.3
    (kworker/2:0-eveIts)  27079      2 1685410443        288    0.0    1.0    1.0
               (dockerd)   1426      1 1684918991     491740    1.0    0.0    1.0
    (kworker/1:2-eveIts)  10237      2 1685409893        838    0.0    1.0    1.0
                  (node)   1710   1697 1685409611       1120    0.7    0.0    0.7
    (kworker/0:2-eveIts)  32158      2 1685410601        130    0.0    0.7    0.7
            (argusagent)  39850      1 1684980673     430058    0.3    0.3    0.7
            (staragentd)  40538      1 1684980691     430040    0.0    0.7    0.7
               (systemd)      1      0 1684918982     491749    0.3    0.3    0.7
              (ilogtail)  38538   1620 1684980652     430079    0.0    0.3    0.3
       (systemd-logind)S   1159      1 1684918989     491742    0.0    0.3    0.3
       (systemd-journalS  38655      1 1684980656     430075    0.0    0.3    0.3
              (logagent)  39295      1 1684980663     430068    0.0    0.3    0.3
             (rcu_sched)     10      2 1684918982     491749    0.0    0.3    0.3
           (ksoftirqd/2)     21      2 1684918982     491749    0.0    0.3    0.3
                 (walle)  40189      1 1684980677     430054    0.3    0.0    0.3
                  (java)  40355      1 1684980680     430051    0.3    0.0    0.3
                  (node)   1755   1710 1685409611       1120    0.0    0.3    0.3
                (docker)  43348  43343 1684980759     429972    0.0    0.3    0.3
           (dbus-daemon)   1125      1 1684918989     491742    0.3    0.0    0.3
    [D-STASK]
             COMMAND    PID   PPID  STACK
          (load_calc)    141    141 [<0>] load_calc_func+0x57/0x130
                                    [<0>] load_calc_func+0x57/0x130
                                    [<0>] kthread+0xf5/0x130
                                    [<0>] ret_from_fork+0x1f/0x30
                                    [<0>] 0xffffffffffffffff

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

    [TIME-STAMP] 2023-05-30 02:07:05
    [UTIL&LOAD]
    usr    sys iowait  load1      R      D   fork : proc 
    11.9   3.3    0.0    3.0      0      1     97 : logagentctl.sh(51049) ppid=51047 cnt=6 
    [PER_CPU]
      cpu    usr    sys iowait  delay(ns)
    cpu-0   11.6    3.0    0.0 320221229182
    cpu-1   12.3    3.3    0.0 2954563205
    cpu-2   12.2    3.3    0.0 5919534403
    cpu-3   11.8    3.9    0.0 30688036677
    [CGROUP]
            cgroup_name      nr_periods    nr_throttled  throttled_time        nr_burst      burst_time
            stress_cg            2531            2523    928824820358               0               0
    [TASKTOP]
               COMMAND    PID   PPID      START        RUN %UTIME %STIME   %CPU
                (test)  42327   1893 1685412142        283   30.0    0.0   30.0
           (AliYunDun)   7999      1 1685411022       1403    1.7    1.7    3.3
            (uniagent)  39728      1 1684980670     431755    2.0    1.0    3.0
            (telegraf)  38654      1 1684980656     431769    1.7    1.0    2.7
                (node)   1816   1710 1685409611       2814    1.7    0.7    2.3
             (tasktop)  23209  23200 1685411589        836    0.3    1.0    1.3
                (node)   1710   1697 1685409611       2814    0.7    0.3    1.0
               (walle)  40189      1 1684980677     431748    0.7    0.0    0.7
     (AliYunDunUpdateS   1793      1 1684918991     493434    0.3    0.0    0.3
     (systemd-journalS  38655      1 1684980656     431769    0.3    0.0    0.3
            (logagent)  39295      1 1684980663     431762    0.3    0.0    0.3
                 (top)  27389  27304 1685411726        699    0.3    0.0    0.3
          (argusagent)  39850      1 1684980673     431752    0.3    0.0    0.3
            (ilogtail)  38538   1620 1684980652     431773    0.3    0.0    0.3
          (staragentd)  40538      1 1684980691     431734    0.0    0.3    0.3
               (dfget)  41456      1 1685412118        307    0.0    0.3    0.3
     (docker-containeS  38593   1426 1684980653     431772    0.3    0.0    0.3
    [D-STASK]
              COMMAND    PID   PPID  STACK
          (load_calc)    141    141 [<0>] load_calc_func+0x57/0x130
                                    [<0>] load_calc_func+0x57/0x130
                                    [<0>] kthread+0xf5/0x130
                                    [<0>] ret_from_fork+0x1f/0x30
                                    [<0>] 0xffffffffffffffff

可以观察到此时虽然**实际负载**很高，大量task由于限流处于R状态，但是由于cgroup机制task并不位于就绪队列中，因此R状态数量指标不准确导致load1计算不准（load1无法准确体现出系统的负载情况）。但是在cgroup限流信息中可以看到stress_cg中**出现了大量的限流**，并且**per-cpu的调度延迟很高**，一定程度体现了cpu就绪队列中存在task堆积。

### 2.5 D状态进程多场景

出现D状态的情况很多，最常见的是由于IO导致进入Uninterrupted Sleep状态。

#### 2.5.1 测试方法

利用stress工具开启64个进程将IO打满。

    stress -i 64

#### 2.5.2 测试结果

    [TIME-STAMP] 2023-05-30 02:37:38
    [UTIL&LOAD]
    usr    sys iowait  load1      R      D   fork : proc 
    5.2   72.4   17.1   15.5     42     31     76 : walle-plugin-no(34901) ppid=40189 cnt=6 
    [PER_CPU]
        cpu    usr    sys iowait  delay(ns)
    cpu-0    5.0   72.2   17.1 16162101481
    cpu-1    6.0   70.4   17.9 13251662817
    cpu-2    3.5   76.8   16.5 20181731711
    cpu-3    5.6   71.2   17.2 16403953008
    [CGROUP]
            cgroup_name      nr_periods    nr_throttled  throttled_time        nr_burst      burst_time
            stress_cg            7811            7801   2868506676781               0               0
    [TASKTOP]
            COMMAND    PID   PPID      START        RUN %UTIME %STIME   %CPU
            (telegraf)  38654      1 1684980656     433602    3.0    2.0    5.0
            (stress)  34147  34109 1685414245         13    0.0    4.7    4.7
            (stress)  34151  34109 1685414245         13    0.0    4.7    4.7
            (stress)  34159  34109 1685414245         13    0.0    4.7    4.7
                                            ...
            (stress)  34125  34109 1685414245         13    0.0    3.7    3.7
            (stress)  34132  34109 1685414245         13    0.0    3.7    3.7
                (node)   1710   1697 1685409611       4647    2.7    0.3    3.0
            (tasktop)  23209  23200 1685411589       2669    0.3    1.0    1.3
        (jbd2/vda2-8)    606      2 1684918988     495270    0.0    1.3    1.3
            (uniagent)  39728      1 1684980670     433588    1.3    0.0    1.3
            (walle)  40189      1 1684980677     433581    1.3    0.0    1.3
        (argusagent)  39850      1 1684980673     433585    0.3    0.7    1.0
        (staragentd)  40538      1 1684980691     433567    0.3    0.7    1.0
            (ilogtail)  38538   1620 1684980652     433606    0.3    0.3    0.7
    (kworker/u8:2-flIsh-  26679      2 1685413999        259    0.0    0.3    0.3
    (kworker/u8:3-flIsh-  28798      2 1685414070        188    0.0    0.3    0.3
    (systemd-logind)S   1159      1 1684918989     495269    0.3    0.0    0.3
            (dockerd)   1426      1 1684918991     495267    0.3    0.0    0.3
                (sshd)   1609   1589 1685409610       4648    0.0    0.3    0.3
    (kworker/0:1H-kbIock    592      2 1684918983     495275    0.0    0.3    0.3
                (node)   1755   1710 1685409611       4647    0.0    0.3    0.3
    (docker-containeS  38593   1426 1684980653     433605    0.3    0.0    0.3
    (kworker/3:1H-kbIock    599      2 1684918983     495275    0.0    0.3    0.3
    (systemd-journalS  38655      1 1684980656     433602    0.0    0.3    0.3
    (logagent-collecS  39369  39295 1684980663     433595    0.3    0.0    0.3
    (kworker/u8:1-flIsh-   6901      2 1685410987       3271    0.0    0.3    0.3
                (node)  13915   1816 1685410008       4250    0.3    0.0    0.3
            (systemd)      1      0 1684918982     495276    0.3    0.0    0.3
        (dbus-daemon)   1125      1 1684918989     495269    0.3    0.0    0.3
    (kworker/u8:4-flIsh-  66011      2 1685412917       1341    0.0    0.3    0.3
    [D-STASK]
            COMMAND    PID   PPID  STACK
        (load_calc)    141    141 [<0>] load_calc_func+0x57/0x130
                                    [<0>] load_calc_func+0x57/0x130
                                    [<0>] kthread+0xf5/0x130
                                    [<0>] ret_from_fork+0x1f/0x30
                                    [<0>] 0xffffffffffffffff
        (jbd2/vda2-8)    606    606 [<0>] jbd2_journal_commit_transaction+0x1356/0x1b60 [jbd2]
                                    [<0>] jbd2_journal_commit_transaction+0x1356/0x1b60 [jbd2]
                                    [<0>] kjournald2+0xc5/0x260 [jbd2]
                                    [<0>] kthread+0xf5/0x130
                                    [<0>] ret_from_fork+0x1f/0x30
                                    [<0>] 0xffffffffffffffff
            (stress)  34110  34110 [<0>] submit_bio_wait+0x84/0xc0
                                    [<0>] submit_bio_wait+0x84/0xc0
                                    [<0>] blkdev_issue_flush+0x7c/0xb0
                                    [<0>] ext4_sync_fs+0x158/0x1e0 [ext4]
                                    [<0>] sync_filesystem+0x6e/0x90
                                    [<0>] ovl_sync_fs+0x36/0x50 [overlay]
                                    [<0>] iterate_supers+0xb3/0x100
                                    [<0>] ksys_sync+0x60/0xb0
                                    [<0>] __ia32_sys_sync+0xa/0x10
                                    [<0>] do_syscall_64+0x55/0x1a0
                                    [<0>] entry_SYSCALL_64_after_hwframe+0x44/0xa9
                                    [<0>] 0xffffffffffffffff
                                        ...
            (stress)  34131  34131 [<0>] submit_bio_wait+0x84/0xc0
                                    [<0>] submit_bio_wait+0x84/0xc0
                                    [<0>] blkdev_issue_flush+0x7c/0xb0
                                    [<0>] ext4_sync_fs+0x158/0x1e0 [ext4]
                                    [<0>] sync_filesystem+0x6e/0x90
                                    [<0>] ovl_sync_fs+0x36/0x50 [overlay]
                                    [<0>] iterate_supers+0xb3/0x100
                                    [<0>] ksys_sync+0x60/0xb0
                                    [<0>] __ia32_sys_sync+0xa/0x10
                                    [<0>] do_syscall_64+0x55/0x1a0
                                    [<0>] entry_SYSCALL_64_after_hwframe+0x44/0xa9
                                    [<0>] 0xffffffffffffffff
可以看到当前存在大量D状态task，并且系统的sys较高，并且抓取到了D-task的内核栈，主要是由stress进程在进行IO导致。

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
