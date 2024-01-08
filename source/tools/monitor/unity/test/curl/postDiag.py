import json
import requests

url = "http://127.0.0.1:8400/api/diag"
params = {"instance" : "127.0.0.1", "nums" : "3"}
body = {"service_name": "jruntime", "params": params}
data = {"body": body}
res = requests.post(url, data=json.dumps(data))
print(res.content, res)