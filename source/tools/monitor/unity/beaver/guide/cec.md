# 告警中心接口

告警中心需要在yaml 里面配置告警中心server 位置，如

```yaml
cec: "http://192.168.0.127"
```

前面的http:// 不能省略

## api

往untiy 推送一个json 数据，

* /api/cec  投递一个事件，含topic data 字段
* /api/alert  投递一个告警，只含 data 内容 会自动添加 SYSOM_SAD_ALERT topic

事件示例：

```json
{
    "topic": "SYSOM_SAD_ALERT", # 投递主题，对于告警固定使用 SYSOM_SAD_ALERT
    "data": {    # SAD 格式的告警数据
      "alert_item": "sysload_high",
      "alert_category": "MONITOR",
      "alert_source_type": "grafana",
      "alert_time": 1694672798174,  # 可选，单位ms，未写会自动添加
      "alert_level": "WARNING",
      "status": "FIRING",
      "labels": {
        "grafana_folder": "rules",
        "alertname": "test"
      },
      "annotations": {
        "summary": "192.168.23.6 实例的系统负载长时间高于100，建议使用系统负载诊断分析sys高的原因"
      },
      "origin_alert_data": {...}
    }
}
```

## 示例代码

以python为例

```python
import json
import requests

url = "http://127.0.0.1:8400/api/alert"
data = {"alert_item": "sysload_high",
    "alert_category": "MONITOR",
    "alert_source_type": "grafana",
    "alert_level": "WARNING",
    "status": "FIRING",
    "labels": {
        "instance": "192.168.23.6",
        "grafana_folder": "rules",
        "alertname": "test"
    }
}
res = requests.post(url, data=json.dumps(data))
print(res.content, res)
```
