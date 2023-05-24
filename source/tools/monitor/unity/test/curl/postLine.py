import requests

url = "http://127.0.0.1:8400/api/line"
line = "lineTable,index=abc value=2"
res = requests.post(url, data=line)
print(res)
