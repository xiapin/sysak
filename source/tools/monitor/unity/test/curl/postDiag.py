import json
import requests

url = "http://127.0.0.1:8400/api/diag"
params = {"instance" : "127.0.0.1"}
body = {"service_name": "iohang", "params": params}
data = {"host" : "192.168.0.121", "uri": "/api/v1/tasks/sbs_task_create/", "body": body}
res = requests.post(url, data=json.dumps(data))
print(res.content, res)