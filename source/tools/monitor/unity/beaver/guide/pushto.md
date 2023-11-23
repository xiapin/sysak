# 推送配置

unity 支持通过yaml 配置将指标、日志等数据向多目标推送。需要确认你当前使用的版本已经更到了最新，早期版本只支持单目标。

支持以下几种推送方式：

* sls （含指标、日志、或两者兼得）
* sls metricstore
* influxdb，含阿里云自研lindorm
* prometheus remote write（预留）

# 配置示例
推送目标配置在yaml 的 pushTo 字段，类型为list，示例如下：

```
  pushTo:
  - to: "Influx"
    host: "www.influx.com"
    port: 8086
    url: "/write?db=db"
  - to: "Sls"
    endpoint: xxxxxx
    project: xxxxxxx
    logstore: xxxxx
    addition: xxxxxx
```

配置说明：
## Sls

将数据以行协议的方式推送到sls logstore，logstore 需要手动创建。参数列表

* endpoint  参考sls endpoint 配置
* project  参考sls project 配置
* logstore  参考sls logstore 配置
* addition 

## SlsLog

参数与Sls 一致，仅推送log

## SlsMetric

参数与Sls 一致，仅推送metric

## Metricstore(推荐使用)

将数据按照 prometheus remote write 方式写入 sls metricstore，参数：
* host  "[project].[endpoint]"
* url "/prometheus/[project]/[metricstore]/api/v1/write"
* addtion

## Metrics

功能与Metricstore 类似，后面将逐步退出

* endpoint  参考sls endpoint 配置
* project  参考sls project 配置
* metricstore  参考sls metricstore 配置
* addition 

## Influx

将数据以行协议方式写入influxDB或者Lindorm，参数

*  host: 目标host
*  port: 数据库端口
*  url: 写入url
	



