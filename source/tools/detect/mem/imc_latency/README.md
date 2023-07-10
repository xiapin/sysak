# imc_latency

基于PMU事件的DDR内存访问延迟，用于检查微架构层级是否存在内存竞争。

## 原理与限制

基于IMC的PMU组件实现，需要硬件支持。目前仅支持Intel的Ice Lake（ICX）、Sky Lake（SKX）、Cascade Lake以及Sapphire Rapids(SPR)等架构。

| micro-architecture | code | cpu-model number |
| ------------------ | ---- | ---------------- |
| Sapphire Rapids    | SPR  | 143              |
| Ice Lake           | ICX  | 106/108          |
| Cascade Lake       |      | 106              |
| Sky Lake-X         | SKX  | 85               |

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
