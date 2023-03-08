# 功能说明
检查系统中cgroup和文件系统cache使用情况

使用示例: sysak memhunter [-df][-c cgroup_name][-n num]
其中：
```
- sysak memhunter -d 扫描泄漏的cgroup内存使用情况
- sysak memhunter -c [cgroup name]  扫描某个cgroup下cache使用情况
- sysak memhunter -f 扫描tmpfs和ext4文件系统下cache使用情况
- 还可以通过-n指令指定输出的条目数（如果输出过多的情况
```

