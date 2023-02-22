

## 基于 eBPF 的周期性采样监控开发手册


我们在 `source/tools/monitor/unity/collector/plugin/bpfsample` 路径提供了一个基于 eBPF 的周期性采样监控开发样例。其主要包含三个部分：

1. Makefile: 用于编译该工具；
2. bpfsample.bpf.c: 此处编写 eBPF 程序
3. bpfsmaple.c: 此处编写用户态程序

接下分别介绍这三个部分。

### Makfile

```Makefile
newdirs := $(shell find ./ -type d)

bpfsrcs := bpfsample.bpf.c
csrcs := bpfsample.c
so := libbpfsample.so

include ../bpfso.mk
```

1. `bpfsrcs`: 用来指定需要编译的 eBPF 程序源文件
2. `csrcs`: 用来指定需要编译的用户态程序源文件
3. `so`: 用来指定生成目标动态库名称

开发者只需要关注上述三个变量的修改即可。


### bpfsample.bpf.c: eBPF 程序的编写

```c
#include <vmlinux.h>
#include <coolbpf.h>
#include "bpfsample.h"

BPF_ARRAY(count, u64, 200);

SEC("kprobe/netstat_seq_show")
int BPF_KPROBE(netstat_seq_show, struct sock *sk, struct msghdr *msg, size_t size)
{
    int default_key = 0;
    u64 *value = bpf_map_lookup_elem(&count, &default_key);
    if (value) {
        __sync_fetch_and_add(value, 1);
    }
    return 0;
}
```

1. `vmlinux.h` 和 `coolbpf.h` 是coolbpf框架提供的两个头文件，里面包含了类似 `BPF_ARRAY` 的helper函数，以及内核结构体的定义
2. `bpfsample.h` 是开发者自定义的头文件


### bpfsample.c: 用户态程序的编写

unity 监控框架提供了三个函数，分别是：

```c
int init(void *arg)
{
    return 0;
}

int call(int t, struct unity_lines *lines)
{
    return 0;
}

void deinit(void)
{
}
```

在 `init` 函数里，需要去 load, attach eBPF程序。为了开发方便，coolbpf提供了简单的宏定义去完成这一系列的操作，即 `LOAD_SKEL_OBJECT(skel_name);` 。因此，一般 `init` 函数具体形式如下：

```c
int init(void *arg)
{
    return LOAD_SKEL_OBJECT(bpf_sample);
}
```

对于 `call` 函数，我们需要周期性去读取 `map` 数据。本样例，在 `call` 函数读取 `count` map里面的数据，去统计事件触发的频次。


```c
int call(int t, struct unity_lines *lines)
{
    int countfd = bpf_map__fd(bpfsample->maps.count);
    int default_key = 0;
    uint64_t count = 0;
    uint64_t default_count = 0;
    struct unity_line* line;

    bpf_map_lookup_elem(countfd, &default_key, &count);
    bpf_map_update_elem(countfd, &default_key, &default_count, BPF_ANY);

    unity_alloc_lines(lines, 1); 
    line = unity_get_line(lines, 0);
    unity_set_table(line, "bpfsample");
    unity_set_value(line, 0, "value", count);

    return 0;
}
```


对于 `deinit` 函数，同 `init` 函数里提供的 `LOAD_SKEL_OBJECT` 宏定义一样，我们也提供了类似的销毁宏定义，即：`DESTORY_SKEL_BOJECT`。 因此，一般 `deinit` 函数具体形式如下：

```c
int deinit(void *arg)
{
    return DESTORY_SKEL_BOJECT(bpf_sample);
}
```