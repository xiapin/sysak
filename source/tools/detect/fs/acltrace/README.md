# 功能说明
ext4文件系统acl跟踪工具

使用说明:
Usage: sysak acltrace [options]
Options:
    --time/-t     specify the monitor period(s), default=5000s
    --help/-h     help info

使用示例：sysak acltrace -t 60 监控60‘s内设置acl情况，其中会申请kmalloc_64,若持续大量的申请，slab内存使用量也会持续增加，最终导致系统可以内存越来越少。
pid             comm            dentry          xattrs                  count           last_time       
30104           setfacl         test8                                   1               2022-09-23 21:17:51
30164           setfacl         test6                                   1               2022-09-23 21:18:05
30007           cp              test8                                   1               2022-09-23 21:16:46
29969           cp              test5                                   1               2022-09-23 21:16:41
30022           cp              test9                                   1               2022-09-23 21:16:47
30023           cp              test0                                   1               2022-09-23 21:16:52
29996           cp              test7                                   1               2022-09-23 21:16:44
30163           setfacl         test7                                   1               2022-09-23 21:18:01
29943           cp              test4                                   1               2022-09-23 21:16:34
29987           cp              test6                                   1               2022-09-23 21:16:43
