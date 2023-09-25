# sar功能说明

收集服务器的系统信息（如cpu，io，mem，tcp等）进行数据统计展示


### 1、cpu
##### 字段含义
* user:表示CPU执行用户进程的时间,通常期望用户空间CPU越高越好
* sys:表示CPU在内核运行时间,系统CPU占用率高,表明系统某部分存在瓶颈.通常值越低越好.
* wait:CPU在等待I/O操作完成所花费的时间.
* hirq: 系统处理硬中断所花费的时间百分比
* sirq: 系统处理软中断所花费的时间百分比
* util: CPU总使用的时间百分比
***

### 2、mem
##### 字段含义
* free: 空闲的物理内存的大小
* used: 已经使用的内存大小
* buff: buff使用的内存大小
* cach: 缓存大小
* total: 系统总的内存大小
* util: 内存使用率
***

### 3、load
##### 字段含义
* load1: 一分钟的系统平均负载
* load5: 五分钟的系统平均负载
* load15:十五分钟的系统平均负载
* runq: 在采样时刻,运行队列的任务的数目,与/proc/stat的procs_running表示相同意思
* plit: 在采样时刻,系统中活跃的任务的个数（不包括运行已经结束的任务）
***

### 4、traffic
##### 字段含义
* bytin: 入口流量byte/s
* bytout: 出口流量byte/s
* pktin: 入口pkt/s
* pktout: 出口pkt/s
* pkterr：发送及接收的错误总数
* pktdrp：设备驱动程序丢弃的数据包总数
***

### 5、tcp
##### 字段含义
* active:主动打开的tcp连接数目
* pasive:被动打开的tcp连接数目
* iseg: 收到的tcp报文数目
* outseg:发出的tcp报文数目
* CurrEs:当前状态为ESTABLISHED的tcp连接数
* retran:系统的重传率
***
### 6、udp
##### 字段含义
* InEr:入口错误数     
* SndEr：发送的错误数       
* In：接收数量
* RcvEr：接收的错误数        
* Out：发送数量           
* NoPort:udp协议层接收到目的地址或目的端口不存在的数据包
***
### 7、io
##### 字段含义
* disk_name:设备名称
* inflight:     
* backlog    
* rmsec    
* util    
* wkb    
* xfers    
* bsize    
* wmsec   
* rkb      
* writes   
* wmerge  
* rmerge  
* reads
***
### 8、partition
##### 字段含义
* path:分区目录
* bfree：分区空闲的字节
* bused: 分区使用中的字节
* btotl: 分区总的大小
* util: 分区使用率
***
### 9、pcsw
##### 字段含义
* block:D状态任务数量
* ctxt：上下文切换次数
* run：并行任务数量


