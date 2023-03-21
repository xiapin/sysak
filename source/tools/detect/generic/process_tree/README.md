# 功能说明
process_tree 是基于eBPF实现用于跟踪进程的继承链的工具。

工具会在执行期间跟踪所有的 fork 调用，并且自动逐级回溯父进程，至多往前追踪十层，**可以用于跟踪指定程序的调用进程（父进程），以及父进程的父进程，以此类推，至多跟踪到 init 进程**

# 使用说明
```bash
sysak process_tree -c <COMM> [-t TIMES]
  [TIMES] 单次诊断持续时间，默认 10s
  [COMM] 要过滤的目标进程的名字
  -c, --comm=FILTER_COMM     要过滤的目标进程的名字，不指定则会输出所有已跟踪到的进程链
  -t, --time=Running time    单次诊断持续时间，默认 10s
  -d, --debug                是否开启 libbpf 的调试输出
  -b, --btf=BTF_PATH         指定自定义BTF文件路径
  -?, --help                 输出帮助说明
      --usage                输出简短版本的使用说明
  -V, --version              输出版本号
```
# 使用举例
## 运行说明

下面的例子使用 process_tree 跟中系统中的进程链，并且过滤出 comm 为 `ls` 的结果，诊断持续10秒（开启一个新的终端执行 `ls -ltrh`） 

```bash
sudo sysak process_tree -c ls -t 10
```

## 输出说明

```bash
=========================1114799(ls)==========================
 1114799(ls, /bin/bash --init-file /root/.vscode-server/bin/441438abd1ac652551dbe4d408dfcec8a499b8bf/out/vs/workbench/contrib/terminal/browser/media/shellIntegration-bash.sh,ls --color=auto -ltrh)
  ↓
  1065507(node, )
    ↓
    1062424(node, )
      ↓
      1062387(sh, )
        ↓
        1062375(bash, )
          ↓
          1062334(bash, )
            ↓
            1062301(sshd, )
              ↓
              1062300(sshd, )
                ↓
                1062298(sshd, )
                  ↓
                  1162(systemd, )
                    ↓
                    1(systemd, )
```

其中每一个进程链输出结果的格式如下：

```bash
=========================<PID>(<COMM>)==========================
 <PID>(<COMM>, <ARGS1>,<ARGS2>,...)
  ↓
  <parent-PID>(<parent-COMM>, <ARGS1>,<ARGS2>,...)
    ↓
    <parent-parent-PID>(<parent-parent-COMM>, <ARGS1>,<ARGS2>,...)
      ↓
      ...
```
- <ARGS1> 为程序的原始参数
- <ARGS2> 如果进程执行过 exec 装载了其它进程，则参数会发生改变（COMM也会变，但是我们只保存了COMM的最新值，参数值则保存了变化历史）
