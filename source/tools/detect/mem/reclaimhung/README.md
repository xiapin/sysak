# 功能说明
内存直接回收规整跟踪工具

使用说明:
Usage: sysak reclaimhung [options]
Options:
    --time/-t     specify the monitor period(s), default=5000s
    --help/-h     help info

使用示例：sysak reclaimhung -t 60 监控60‘s内内存直接回收规整情况
Reclaim:
pid                     name                    nr_reclaimed            last_delay(ns)          last_time                               nr_pages        
345131                  AliYunDunUpdate         3                       211520461               2022-08-09 21:56:31(2524618984216187)   181             
345162                  AliYunDun               28                      67159873                2022-08-09 21:56:31(2524619133502314)   863             
345164                  AliYunDun               14                      210923864               2022-08-09 21:56:31(2524619000048068)   383             
1101                    in:imjournal            1                       209810227               2022-08-09 21:56:28(2524616672950622)   45              
483305                  malloc_500M.out         3                       207973923               2022-08-09 21:56:29(2524617843688133)   105             
345176                  AliYunDun               17                      211724346               2022-08-09 21:56:31(2524619000019646)   303             
1191                    exe                     1                       104457204               2022-08-09 21:56:31(2524619082400326)   39              
16986                   aliyun-service          3                       207419914               2022-08-09 21:56:31(2524618987386484)   176             
345168                  AliYunDun               22                      205958029               2022-08-09 21:56:31(2524619005065051)   412             
996                     tuned                   1                       109400290               2022-08-09 21:56:31(2524619086391892)   45              
483304                  malloc_500M.out         4                       315659267               2022-08-09 21:56:28(2524616591023615)   230             
478338                  sshd                    1                       210528191               2022-08-09 21:56:31(2524619000222252)   50              
345185                  AliYunDunUpdate         1                       104190192               2022-08-09 21:56:31(2524619090608611)   62              
Compaction:
pid                     name                    nr_compacted            last_delay(ns)          last_time                               result          
Cgroup Reclaim:
pid                     name                    nr_reclaimed            last_delay(ns)          last_time                               nr_page
