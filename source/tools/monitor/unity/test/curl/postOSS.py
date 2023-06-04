
import requests
import uuid
import json

url = "http://127.0.0.1:8400/api/oss"
d = {"stream": "hello oss", "uuid": str(uuid.uuid4())}
res = requests.post(url, json=d)
print(res)
