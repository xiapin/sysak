import json
import requests

url = "http://127.0.0.1:8400/api/diag"
params = {"instance" : "127.0.0.1"}
body = {"service_name": "memgraph", "params": params}
data = {"body": body}
res = requests.post(url, data=json.dumps(data))
print(res.content, res)