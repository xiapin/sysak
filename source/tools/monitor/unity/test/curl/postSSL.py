import json
import requests

url = "http://127.0.0.1:8400/api/ssl"
data = {"host": "cn.bing.com", "uri": "/"}
res = requests.post(url, data=json.dumps(data))
print(res.content, res)