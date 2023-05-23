import requests
import json
import uuid

url = "http://127.0.0.1:8400/api/trig"
vs = {"cmd": "diag", "exec": "jruntime", "args": ["-d", "5", "--top", "2"], "uid": str(uuid.uuid4())}
res = requests.post(url, json=vs)
print(res)
