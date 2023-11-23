# 功能说明
sysctl 用于检查fd,pid,inode,rootfs等资源使用情况工具。

# 使用说明
```bash
Usage: [fd|pid|root|inode] [threshold]
fd: fd使用量检查
pid: pid使用量检查
inode: inode 使用量检查
threshold: 相对于max limit的百分比
```
# 使用举例
## 运行说明

下面的例子使用sysctl 检查fd 的使用量

```bash
sudo sysak sysctl fd 0.6
```

## 输出说明
file-max: 系统file max
file-used: 系统当前fd使用量
pid-max: 单个进程可打开最大fd
后面是系统fd 使用top 10
pid:进程号
comm: 进程明
fd: fd数量
```bash
file-max:1606698 file-used:1401664 pid-max:655350
pid: 21308, comm: test, fd: 300004
pid: 22039, comm: main, fd: 300004
pid: 22070, comm: java, fd: 300004
pid: 21171, comm: java, fd: 200004
pid: 20595, comm: stress, fd: 200003
pid: 21235, comm: stress, fd: 100004
pid: 611, comm: java, fd: 88
pid: 2383, comm: java, fd: 75
pid: 1, comm: systemd, fd: 49
pid: 1371, comm: java, fd: 42
```
