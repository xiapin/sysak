

## 基于 eBPF 的事件监控开发手册


我们在 `source/tools/monitor/unity/collector/plugin/bpfsample2` 路径提供了一个基于 eBPF 的监控开发样例。其主要包含三个部分：

1. Makefile: 用于编译该工具；
2. bpfsample2.bpf.c: 此处编写 eBPF 程序
3. bpfsmaple2.c: 此处编写用户态程序

接下分别介绍这三个部分。

### Makfile

```Makefile
newdirs := $(shell find ./ -type d)

bpfsrcs := bpfsample2.bpf.c
csrcs := bpfsample2.c
so := libbpfsample2.so

include ../bpfso.mk
```

1. `bpfsrcs`: 用来指定需要编译的 eBPF 程序源文件
2. `csrcs`: 用来指定需要编译的用户态程序源文件
3. `so`: 用来指定生成目标动态库名称

开发者只需要关注上述三个变量的修改即可。


### bpfsample2.bpf.c: eBPF 程序的编写

```c
#include <vmlinux.h>
#include <coolbpf.h>
#include "bpfsample2.h"

BPF_PERF_OUTPUT(perf, 1024);

SEC("kprobe/netstat_seq_show")
int BPF_KPROBE(netstat_seq_show, struct sock *sk, struct msghdr *msg, size_t size)
{
    struct event e = {};

    e.ns = ns();
    e.cpu = cpu();
    e.pid = pid();
    comm(e.comm);
    
    bpf_perf_event_output(ctx, &perf, BPF_F_CURRENT_CPU, &e, sizeof(struct event));
    return 0;
}

```

1. `vmlinux.h` 和 `coolbpf.h` 是coolbpf框架提供的两个头文件，里面包含了类似 `BPF_PERF_OUTPUT` 的helper函数，以及内核结构体的定义
2. `bpfsample2.h` 是开发者自定义的头文件


### bpfsample2.c: 用户态程序的编写

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

在 `init` 函数里，需要去 load, attach eBPF程序，如有需要可能还会创建用于接收perf事件的线程。为了开发方便，coolbpf提供了简单的宏定义去完成这一系列的操作，即 `LOAD_SKEL_OBJECT(skel_name, perf);` 。因此，一般 `init` 函数具体形式如下：

```c
int init(void *arg)
{
    return LOAD_SKEL_OBJECT(bpf_sample2, perf);;
}
```

对于 `call` 函数，我们保持不变，即直接 `return 0`。

对于 `deinit` 函数，同 `init` 函数里提供的 `LOAD_SKEL_OBJECT` 宏定义一样，我们也提供了类似的销毁宏定义，即：`DESTORY_SKEL_BOJECT`。 因此，一般 `deinit` 函数具体形式如下：

```c
int deinit(void *arg)
{
    return DESTORY_SKEL_BOJECT(bpf_sample2);
}
```