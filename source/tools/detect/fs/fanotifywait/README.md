# 功能说明
文件访问监听工具

使用说明:
```
Usage: sysak fanotifywait [options] [args]
Options:
    -s          specify how long to run, default= 60s
    -f          specify monitor file or dir
    -h          help info
example:
sysak fanotifywait -f ./test.txt -s 10  #monitor file
sysak fanotifywait -f ./test -s 10  #monitor file
```


效果示例：
```
2023/3/9 9:26:25  cat:507563  OPEN  /root/sysak/rpm/sysak.service
2023/3/9 9:26:25  cat:507563  ACCESS  /root/sysak/rpm/sysak.service
2023/3/9 9:26:25  cat:507563  ACCESS  /root/sysak/rpm/sysak.service
2023/3/9 9:26:25  cat:507563  CLOSE_NOWRITE  /root/sysak/rpm/sysak.service
2023/3/9 9:26:25  cat:507668  OPEN  /root/sysak/rpm/sysak-build.sh
2023/3/9 9:26:25  cat:507668  ACCESS  /root/sysak/rpm/sysak-build.sh
2023/3/9 9:26:25  cat:507668  ACCESS  /root/sysak/rpm/sysak-build.sh
2023/3/9 9:26:25  cat:507668  CLOSE_NOWRITE  /root/sysak/rpm/sysak-build.sh      
```

