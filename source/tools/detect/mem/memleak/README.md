# 功能说明
检查内核Slab，Page，Vmalloc是否存在泄漏以及动态跟踪泄漏函数。

编译：
./configure --enable-lkm --enable-target-memleak
memleak目前通过内核模块实现，在线上环境需要验证后再使用。

参数说明
sysak memleak -h
-t 
  slab: trace slab  leak
  page: trace alloc page  leak
  vmalloc: trace vmalloc  leak, must use root 
-i: trace internal,default 300s 
-s: stacktrace for memory alloc 
-d: memleak off 
-c: only check memleak,don't diagnose 
-n: trace slab name, defualt select the max size or objects 

-t: 检测泄漏类型，目前支持slab，page，vmalloc三种内存泄漏检测
-i: 动态监测时间（默认300秒）
-s:   保存内存调用栈(slab类型有效)
-c:   快速检测系统是否存在内存泄漏
-n:  检测slab类型（slabt类型有效）

内存泄漏检测:
sysak memleak -c

allocPages:339M, uslab:59M vmalloc:300M
诊断结论: no memleak
结果分析：分别展示了page内存，slab Unreclaim内存，以及vmalloc内存，并给出是否存在内存泄漏的结论

诊断slab泄漏:
    sysak memleak -t slab -i 300
    指定slab名为kmalloc-128
    sysak memleak -t slab -n kmalloc-128 -i 300
结果解析:
重点看未释放内存详细列表，未释放内存汇总和疑似泄漏函数
sysak memleak -t slab -i 100

未释放内存详细列表:
ilogtail:55156            ext4_inode_attach_jinode.part.75+0x28/0xa0 [ext4]  ptr=0xffffa06d198a5ec0 mark 1 delta = 47
ilogtail:55156            ext4_inode_attach_jinode.part.75+0x28/0xa0 [ext4]  ptr=0xffffa06d198a5a80 mark 1 delta = 75

未释放内存汇总:
次数    标记次数       函数
2        2       ext4_inode_attach_jinode.part.75+0x28/0xa0 [ext4]

疑似泄漏函数: ext4_inode_attach_jinode.part.75+0x28/0xa0 [ext4]

诊断page泄漏
sysak memleak -t page -i 300
诊断vmalloc泄漏
sysak memleak -t vmalloc
