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