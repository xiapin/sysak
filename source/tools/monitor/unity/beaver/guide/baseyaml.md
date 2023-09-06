# site
/etc/sysak/base.yaml
# 各字段含义
````
config:
  freq: 20  # 采集间隔 
  port: 8400 # 监听端口
  bind_addr: 0.0.0.0  # 监听IP
  backlog: 32 # 服务监听队列长度
  url_safe: close # 只开放必要的url，
  identity: # 实例id配置模式，当前支持以下五种模式
            # hostip: 获取主机IP
            # curl: 通过网络请求获取，需要指定url 参数，适合ECS场景
            # file: 从文件读取，需要指定path 参数	
            # specify:	指定id，需要指定name参数
    mode: hostip
    name: test_specify
  real_timestamps: true # 上报监测数据的真实时间，默认关闭
 # unix_socket: "/tmp/sysom_unity.sock" # 通过unix_socket方式进行数据传输，默认关闭
  proc_path: /mnt/host/  # proc 文件路径，在host侧，为/。在容器侧，如配置 -v /:/mnt/host 则为 /mnt/host
  limit: # 资源限制
    cpu: 30
    mem: 60 
    cellLimit: -1 # guardSched插件执行时长，值为-1表示不限制时长，若不设置，则默认为50ms
 
outline:  # 外部数据入口，适合接入外部数据场景
  - /tmp/sysom  # 外部unix socket 路径，可以指定多个

luaPlugins: ["proc_buddyinfo", "proc_diskstats", "proc_meminfo", "proc_mounts", "proc_netdev",
            "proc_snmp_stat", "proc_sockstat", "proc_stat", "proc_statm", "proc_vmstat"]  # 控制lua 插件加载
 
pushTo: # 向指定数据库推送数据
  to: "AutoMetrics" # 推送数据的模式，当前支持以下三种模式
                    # "AutoMetrics"，推送到metricstore，自动识别project，endpoint和metricstore，需要配置addition
                    # "Metrics"，推送到metricstore，需要设置project，endpoint，metricstore和addition
                    # "Influx"，推送到Influxdb，需设置host，port和url
  addition: "***"   # 加密后的账户信息
  project: "sysom-metrics-cn-somewhere" # metricstore的project名
  endpoint: "cn-somewhere-intranet.log.aliyuncs.com" # metricstore的域名
  metricstore: "auto" # metricstore的时序数据库名
  host: "xxx.aliyuncs.com"
  port: 8242
  url: "/api/v2/write?db=sysom"

plugins:  # 插件列表 对应 /collector/plugin 路径下编译出来的c库文件
  - so: kmsg  # 库名
    description: "collect dmesg info."  # 描述符

metrics:   # export 导出的 metrics 列表
  -
    title: sysak_proc_cpu_total  # 显示的表名
    from: cpu_total   # 数据源头，对应collector生成的数据表
    head: mode        # 字段名，在prometheus 中以label 方式呈现
    help: "cpu usage info for total." # help 说明
    type: "gauge"     # 数据类型
    discrete: true # 数据是否为离散的，不定义则默认为false
    blacklist: 设置数据上传的黑名单，黑名单和白名单不可同时设置
      cpu: "cpu1$" # 按照lua的正则表达式进行设置 https://www.cnblogs.com/meamin9/p/4502461.html
      value: "block"
    whitelist: 设置数据上传的白名单，黑名单和白名单不可同时设置
      cpu: "cpu1$" # 
      value: "block"

observe:  # 数据观测设置
  comms:  # 需要观测的进程名
    java: "cgroup xxx" #需要获取的参数，双引号内的参数名用空格隔开
    mysqld: "cgroup"
    ···
  period: 60  # 监测进程的刷新间隔
 
diagnose:  # 诊断功能设置
  host:  # sysom中心端ip，example http：//111.111.111.111
  token: ""  访问sysom中心端的加密后的token
  jobs:  # 具体诊断项目的设置，不设置会将对应的block和time设置为默认值
    memgraph:  #  诊断的service_name
      block: 60  # 阻塞时间，单位秒
      time: 30   # 执行时间，单位秒
````

