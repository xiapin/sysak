
import requests
import uuid
import json

url = "http://127.0.0.1:8400/api/oss"
headers = {'Content-Type': 'application/octet-stream', 'uuid': str(uuid.uuid4())}
data = b'binary data'

response = requests.post(url, headers=headers, data=data)
print(response)
