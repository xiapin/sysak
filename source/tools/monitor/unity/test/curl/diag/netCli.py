import requests
import json
import uuid

url = "http://127.0.0.1:8400/api/trig"
vs = {"cmd": "diag", "exec": "net_edge", "args": ["11.0.145.174", "host", "tcp port 80"], "uid": str(uuid.uuid4())}
res = requests.post(url, json=vs)
print(res)
