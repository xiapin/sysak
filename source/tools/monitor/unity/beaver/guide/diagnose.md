# 诊断接口
[url](http://127.0.0.1:8400/api/diag)

上传诊断命令时需要带上对应的参数

具体如下
## iohang

### 需要的参数

instance:需要诊断的实例IP

timeout：诊断时长，单位秒

threshold：保留IO HANG住时间超过阈值的IO,单位毫秒

disk：需要诊断的目标磁盘，缺省为所有磁盘


### 诊断示例
```python
import json
import requests

url = "http://127.0.0.1:8400/api/diag"
params = {"instance" : "127.0.0.1", "timeout" : 5, "threshold" : 10}
body = {"service_name": "iohang", "params": params}
data = {"body": body}
res = requests.post(url, data=json.dumps(data))
print(res.content, res)
```
## iofsstat
### 需要的参数
instance:需要诊断的实例IP

timeout：诊断时长,也是IO流量统计周期,单位秒,建议不超过60秒

disk：需要诊断的目标磁盘，缺省为所有磁盘

### 诊断示例
```python
import json
import requests

url = "http://127.0.0.1:8400/api/diag"
params = {"instance" : "127.0.0.1", "timeout" : 5}
body = {"service_name": "iofsstat", "params": params}
data = {"body": body}
res = requests.post(url, data=json.dumps(data))
print(res.content, res)
```

## iolatency
### 需要的参数
instance:需要诊断的实例IP

timeout：诊断时长,单位秒

threshold：保留IO延迟大于设定时间阈值的IO，单位毫秒

disk：需要诊断的目标磁盘，缺省为所有磁盘

### 诊断示例
```python
import json
import requests

url = "http://127.0.0.1:8400/api/diag"
params = {"instance" : "127.0.0.1", "timeout" : 5, "threshold" : 10}
body = {"service_name": "iolatency", "params": params}
data = {"body": body}
res = requests.post(url, data=json.dumps(data))
print(res.content, res)
```


## jitter
### 需要的参数
instance:需要诊断的实例IP

time：诊断时长,单位秒

### 诊断示例
```python
import json
import requests

url = "http://127.0.0.1:8400/api/diag"
params = {"instance" : "127.0.0.1", "time" : 5}
body = {"service_name": "jitter", "params": params}
data = {"body": body}
res = requests.post(url, data=json.dumps(data))
print(res.content, res)
```

## loadtask

### 需要的参数
instance：诊断的实例IP

### 诊断示例
```python
import json
import requests

url = "http://127.0.0.1:8400/api/diag"
params = {"instance" : "127.0.0.1"}
body = {"service_name": "loadtask", "params": params}
data = {"body": body}
res = requests.post(url, data=json.dumps(data))
print(res.content, res)
```

## memgraph

### 需要的参数
instance：诊断的实例IP

### 诊断示例
```python
import json
import requests

url = "http://127.0.0.1:8400/api/diag"
params = {"instance" : "127.0.0.1"}
body = {"service_name": "memgraph", "params": params}
data = {"body": body}
res = requests.post(url, data=json.dumps(data))
print(res.content, res)
```

## filecache

### 需要的参数
instance：诊断的实例IP

value：需要诊断的容器ID,Pod名,cgroup

type：需要诊断的类型(容器,POD,cgroup, host, all(所有容器))

### 诊断示例
```python
import json
import requests

url = "http://127.0.0.1:8400/api/diag"
params = {"instance" : "127.0.0.1", "value" : "30001a90d0ff", "type" : "container"}
body = {"service_name": "filecache", "params": params}
data = {"body": body}
res = requests.post(url, data=json.dumps(data))
print(res.content, res)
```

## oomcheck
### 需要的参数
instance：诊断的实例IP

time：需要诊断OOM的时间点，默认为最近一次

### 诊断示例
```python
import json
import requests

url = "http://127.0.0.1:8400/api/diag"
params = {"instance" : "127.0.0.1"}
body = {"service_name": "oomcheck", "params": params}
data = {"body": body}
res = requests.post(url, data=json.dumps(data))
print(res.content, res)
```
## ossre

## packetdrop
### 需要的参数
instance:需要诊断的实例IP

time：诊断时长,单位秒

### 诊断示例
```python
import json
import requests

url = "http://127.0.0.1:8400/api/diag"
params = {"instance" : "127.0.0.1", "time" : 5}
body = {"service_name": "packetdrop", "params": params}
data = {"body": body}
res = requests.post(url, data=json.dumps(data))
print(res.content, res)
```

## pingtrace
### 需要的参数
origin_instance：源实例IP

target_instance：目标实例IP

pkg_num：追踪包数

time_gap：间隔毫秒数

type：报文协议(icmp,tcp,udp)

### 诊断示例
```python
import json
import requests

url = "http://127.0.0.1:8400/api/diag"
params = {"origin_instance" : "127.0.0.1", "target_instance" : "192.168.0.1", "pkg_num" : 5, "time_gap" : 10, "type" : "icmp"}
body = {"service_name": "pingtrace", "params": params}
data = {"body": body}
res = requests.post(url, data=json.dumps(data))
print(res.content, res)
```



## retran
### 需要的参数
instance:需要诊断的实例IP

time：诊断时长,单位秒

### 诊断示例
```python
import json
import requests

url = "http://127.0.0.1:8400/api/diag"
params = {"instance" : "127.0.0.1", "time" : 5}
body = {"service_name": "retran", "params": params}
data = {"body": body}
res = requests.post(url, data=json.dumps(data))
print(res.content, res)
```

## schedmoni

### 需要的参数
instance:需要诊断的实例IP

time：本次的期望的诊断时间，默认20秒

### 诊断示例
```python
import json
import requests

url = "http://127.0.0.1:8400/api/diag"
params = {"instance" : "127.0.0.1", "time" : 5}
body = {"service_name": "schedmoni", "params": params}
data = {"body": body}
res = requests.post(url, data=json.dumps(data))
print(res.content, res)
```

## taskprofile

### 需要的参数
instance:需要诊断的实例IP

timeout：诊断时长,也是各应用占用cpu统计周期,单位分钟,建议不超过10分钟

### 诊断示例
```python
import json
import requests

url = "http://127.0.0.1:8400/api/diag"
params = {"instance" : "127.0.0.1", "timeout" : 1}
body = {"service_name": "taskprofile", "params": params}
data = {"body": body}
res = requests.post(url, data=json.dumps(data))
print(res.content, res)
```
## jruntime

### 需要的参数
instance:需要诊断的实例IP

nums：要诊断的进程数量，但是格式为字符串

### 诊断示例
```python
import json
import requests

url = "http://127.0.0.1:8400/api/diag"
params = {"instance" : "127.0.0.1", "nums" : "3"}
body = {"service_name": "jruntime", "params": params}
data = {"body": body}
res = requests.post(url, data=json.dumps(data))
print(res.content, res)
```