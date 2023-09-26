# 功能说明

mysql应用观测服务agent，以应用为视角，集指标、追踪、事件分析、日志分析的Mysql可观测程序，针对mysql服务端场景，具备收集mysql应用、系统侧监控指标，异常事件、以及打通SysOM事件根因诊断分析的能力

# 功能依赖

sql-obs依赖sysak unity监控组件，在机器上部署之后，可直接通过systemctrl start sysak的方式启动

- 此程序为mysql应用观测agent，应用观测的最终UI界面呈现，安装SysOM使用
  

# 使用说明

在unity的yaml配置文件中，路径：source/tools/monitor/unity/etc/base.yaml中，forkRun标签中添加sql-obs的启动配置，由于默认配置文件中已经配置好，因此启动sysak服务情况下，mysql观测agent会默认启动，但默认配置不包含mysql数据库监控账户信息，因此不修改配置情况下，拿不到mysql应用侧指标；配置方法如下：

- u参数表示指定用户名明文（或存在s参数情况下，为加密后的用户名）
- p参数表示指定密码明文（或存在s参数情况下，为加密后的密码）
- s参数表示账户密码信息是否加密，目前支持的是base64

如：

```
forkRun:
...
  - cmd: "../../../sql-obs"
    args:
      ["-u", "用户名密文", "-p", "密码密文", "-s", "BASE"]
...
```

或

```
forkRun:
...
  - cmd: "../../../sql-obs"
    args:
      ["-u", "用户名明文", "-p", "密码明文"]
...
```

推荐第一种，以上配置前提是，需要在你的数据库中配置合理权限的监控账户，一般需要配置上PROCESS权限

# 功能详情

## 指标说明

### 异常告警统计指标

- Alarm_Process_Mysql_Error_Type, mysql错误告警次数统计
- Alarm_Process_Mysql_Slow_Sql_Type, mysql慢查询次数统计
- Alarm_Process_OOM_Type, mysql被系统OOM杀掉次数统计
- Alarm_Process_RT_Type, mysql RT高次数统计
- Alarm_Process_Sched_Delay_Type, mysql调度延迟高次数统计
- Alarm_Direct_Reclaim_Type, mysql内存申请被阻塞次数统计
- Alarm_Process_CPU_HIGH_Type, mysql CPU高占用次数统计

### Mysql资源消耗指标统计

- Mysql连接池使用详情
- Mysql线程池使用详情
- Mysql CPU占用率
- Mysql CPU让出率
- Mysql Undolog链表长度&长事务统计
- Mysql buffer pool使用详情
- Mysql内存使用布局
- Mysql redolog使用详情

### Mysql延迟相关指标

- Mysql RT指标
- Mysql请求详情表
- Mysql等待IO资源延迟
- Mysql申请OS内存延迟
- Mysql OS调度延迟
- Mysql IO处理延迟分布

### Mysql吞吐带宽相关指标

- Mysql网络吞吐
- Mysql请求数
- Mysql IO吞吐
- Mysql OS脏页统计

## 异常事件说明

### 应用侧事件

- 慢查询事件检测、原因分析、SysOM根因诊断联动

### 系统侧事件

- Mysql IO高事件、IO延迟高事件检测、原因分析、SysOM根因诊断联动
- Mysql RT高事件、调度延迟高、CPU占用高事件检测、原因分析、SysOM根因诊断联动
- Mysql OOM事件、申请内存受阻事件检测、原因分析、SysOM根因诊断联动
