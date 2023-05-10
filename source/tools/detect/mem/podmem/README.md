# 功能说明
podmem用于分析容器，容器组，整机，cgroup的cache内存由哪些文件引入
以及每个文件引入的active cache和inactive cache。
一 工具使用
```
 podmem help指令
指令: sysak podmem -h
-p: analysis pod pagecache(sysak podmem -p ack-node-problem-detector)
-c: analysis container pagecache(sysak podmem -c bd2146176a5ce)
-a: analysis all container pagecache(sysak podmem -f /sys/fs/cgroup/memory/system.slice)
-f: analysis cgroup pagecache(sysak podmem -f /sys/fs/cgroup/memory/system.slice)
-s: analysis system pagecache(sysak podmem -s )
-j: dump result to json file (sysak podmem -s -j ./test.json)
-r: set sample rate ,default set to 1 (sysak podmem -s -r 2)
-t: output filecache top ,default for top 10 (sysak podmem -s -t 20)
其中：
-p 表示分析pod 的cache内存
-c 表示分析容器的cache内存
-f  表示分析cgroup的cache内存
-s  分析整机的cache内存
-a  分析系统所有容器的cache内存
-t   输出结果top数量，默认是top10
-r   设置程序采用率，如果想要工具性能损耗低，可以设置 -r 100,表示每100个页面中，采样一个页面(4k)
-j    以json格式输出
```
二 结果解析
比如执行：sysak podmem -a分析系统上所有容器的cache内存
输出如下：
```
container id: 8cc6b67f627c7 podname:  nginx-5dc47844dd-wm8mg
/var/lib/containerd/io.containerd.snapshotter.v1.overlayfs/snapshots/76/fs/usr/lib/x86_64-linux-gnu/libcrypto.so.1.0.0
size: 2000 cached: 1864 active: 0 inactive: 1864 shmem: 0 delete: 0
container id：容器ID
podname: 容器组名
文件名libcrypto.so.1.0.0
size: 文件大小(单位KB)
cached: 文件引入的cache大小(单位KB)
active: 文件活跃cache大小
inactive: 文件非活跃cache大小
shmem: 文件是否属于共享内存
delete: 文件是否被删除
```
