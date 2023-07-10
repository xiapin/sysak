# imc_latency

基于PMU事件的DDR内存访问延迟，用于检查微架构层级是否存在内存竞争。

## Usgae

### 使用用例

```bash
Sample:

imc_latency -f /dev/stdout #输出日子到控制台
imc_latency -d 15 i 20  # 每15秒采集一次 输出20次采集结果
```

### 结果说明

一次的采集结果如下，输出的的指标类型由read_latency(rlat)和write_latency(wlat)，指标的level有socket和channel两种级别。

- SOCKET_LEVEL： socket层级的读写内存延迟，通过对channel级的指标求平均得到。
- CHANNEL_LEVEL：channel级别的读写内存延迟
  
```bash
[TIME-STAMP] 2023-07-10 07:06:17
[SOCKET_LEVEL]
               0       1
    rlat   13.75   14.37
    wlat   39.37   37.49
[CHANNEL_LEVEL]-[SOCKET-0]
               0       1       2       3       4       5       6       7       8       9      10      11
    rlat   14.37   13.75    0.00   13.75   13.75    0.00   13.12   13.75    0.00   14.37   13.75    0.00
    wlat   40.62   39.99    0.00   39.37   38.74    0.00   40.62   39.37    0.00   39.99   38.74    0.00
[CHANNEL_LEVEL]-[SOCKET-1]
               0       1       2       3       4       5       6       7       8       9      10      11
    rlat   15.00   13.75    0.00   13.75   13.75    0.00   13.75   14.37    0.00   14.37   14.37    0.00
    wlat   38.12   37.49    0.00   36.87   36.87    0.00   38.12   38.12    0.00   38.12   37.49    0.00
```

## 原理与限制

基于IMC的PMU组件实现，需要硬件支持。目前仅支持Intel的Ice Lake（ICX）、Sky Lake（SKX）、Cascade Lake以及Sapphire Rapids(SPR)等架构。

| 微架构          | 代号 | cpu-model编号 |
| --------------- | ---- | ------------- |
| Sapphire Rapids | SPR  | 143           |
| Ice Lake        | ICX  | 106/108       |
| Cascade Lake    |      | 106           |
| Sky Lake-X      | SKX  | 85            |

### 检查是否支持

可以通过`lscpu`的`Model`字段检查硬件是否支持。

```bash
Architecture:        x86_64
CPU op-mode(s):      32-bit, 64-bit
Byte Order:          Little Endian
CPU(s):              128
On-line CPU(s) list: 0-127
Thread(s) per core:  2
Core(s) per socket:  32
Socket(s):           2
NUMA node(s):        2
Vendor ID:           GenuineIntel
BIOS Vendor ID:      Intel(R) Corporation
CPU family:          6
Model:               106
Model name:          Intel(R) Xeon(R) Platinum 8369B CPU @ 2.90GHz
BIOS Model name:     Intel(R) Xeon(R) Platinum 8369B CPU @ 2.90GHz
Stepping:            6
CPU MHz:             3500.000
CPU max MHz:         3500.0000
CPU min MHz:         800.0000
BogoMIPS:            5800.00
Virtualization:      VT-x
L1d cache:           48K
L1i cache:           32K
L2 cache:            1280K
L3 cache:            49152K
NUMA node0 CPU(s):   0-31,64-95
NUMA node1 CPU(s):   32-63,96-127
```
